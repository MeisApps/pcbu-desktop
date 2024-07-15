#include "TCPUnlockClient.h"

#include "connection/SocketDefs.h"
#include "storage/AppSettings.h"

#ifdef WINDOWS
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

TCPUnlockClient::TCPUnlockClient(const std::string& ipAddress, int port, const PairedDevice& device)
    : BaseUnlockServer(device) {
    m_IP = ipAddress;
    m_Port = port;
    m_ClientSocket = (SOCKET)SOCKET_INVALID;
    m_IsRunning = false;
}

bool TCPUnlockClient::Start() {
    if(m_IsRunning)
        return true;

#ifdef WINDOWS
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        spdlog::error("WSAStartup failed.");
        return false;
    }
#endif
    m_IsRunning = true;
    m_AcceptThread = std::thread(&TCPUnlockClient::ConnectThread, this);
    return true;
}

void TCPUnlockClient::Stop() {
    if(!m_IsRunning)
        return;

    if(m_ClientSocket != SOCKET_INVALID && m_HasConnection)
        write(m_ClientSocket, "CLOSE", 5);

    m_IsRunning = false;
    m_HasConnection = false;
    SOCKET_CLOSE(m_ClientSocket);
    if(m_AcceptThread.joinable())
        m_AcceptThread.join();
}

void TCPUnlockClient::ConnectThread() {
    std::string serverDataStr{};
    Packet responsePacket{};
    PacketError writeResult{};
    uint32_t numRetries{};
    auto settings = AppSettings::Get();
    spdlog::info("Connecting via TCP...");

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((u_short)m_Port);
    if (inet_pton(AF_INET, m_IP.c_str(), &serv_addr.sin_addr) <= 0) {
        spdlog::error("Invalid IP address.");
        m_IsRunning = false;
        m_UnlockState = UnlockState::UNK_ERROR;
        return;
    }

    socketStart:
    if ((m_ClientSocket = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_INVALID) {
        spdlog::error("socket() failed. (Code={})", SOCKET_LAST_ERROR);
        m_IsRunning = false;
        m_UnlockState = UnlockState::UNK_ERROR;
        return;
    }

    fd_set fdSet{};
    FD_SET(m_ClientSocket, &fdSet);
    struct timeval socketTimeout{}, connectTimeout{};
    socketTimeout.tv_sec = (long)settings.clientSocketTimeout;
    connectTimeout.tv_sec = (long)settings.clientConnectTimeout;

#ifdef WINDOWS
    auto timeoutMs = (DWORD)(socketTimeout.tv_sec * 1000);
    if (setsockopt(m_ClientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs)) < 0 ||
        setsockopt(m_ClientSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs)) < 0) {
        spdlog::error("setsockopt() for timeout failed. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }
#else
    if (setsockopt(m_ClientSocket, SOL_SOCKET, SO_RCVTIMEO, &socketTimeout, sizeof(socketTimeout)) < 0 ||
        setsockopt(m_ClientSocket, SOL_SOCKET, SO_SNDTIMEO, &socketTimeout, sizeof(socketTimeout)) < 0) {
        spdlog::error("setsockopt() for timeout failed. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }
#endif
    if(!SetSocketBlocking(m_ClientSocket, false)) {
        spdlog::error("Failed setting socket to non-blocking mode. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }
    if(connect(m_ClientSocket, reinterpret_cast<struct sockaddr *>(&serv_addr), sizeof(serv_addr)) < 0) {
        auto error = SOCKET_LAST_ERROR;
        if(error != SOCKET_ERROR_IN_PROGRESS && error != SOCKET_ERROR_WOULD_BLOCK) {
            spdlog::error("connect() failed. (Code={})", error);
            m_UnlockState = UnlockState::CONNECT_ERROR;
            goto threadEnd;
        }
    }
    if(select((int)m_ClientSocket + 1, nullptr, &fdSet, nullptr, &connectTimeout) <= 0) {
        spdlog::error("select() timed out or failed. (Code={}, Retry={})", SOCKET_LAST_ERROR, numRetries);
        if(numRetries < settings.clientConnectRetries && m_IsRunning) {
            SOCKET_CLOSE(m_ClientSocket);
            numRetries++;
            goto socketStart;
        }
        m_UnlockState = UnlockState::CONNECT_ERROR;
        goto threadEnd;
    }

    m_HasConnection = true;
    serverDataStr = GetUnlockInfoPacket();
    if(serverDataStr.empty()) {
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }

    writeResult = WritePacket(m_ClientSocket, {serverDataStr.begin(), serverDataStr.end()});
    if(writeResult == PacketError::NONE) {
        responsePacket = ReadPacket(m_ClientSocket);
        OnResponseReceived(responsePacket);
    } else {
        switch (writeResult) {
            case PacketError::CLOSED_CONNECTION:
                m_UnlockState = UnlockState::CONNECT_ERROR;
                break;
            case PacketError::TIMEOUT:
                m_UnlockState = UnlockState::TIMEOUT;
                break;
            default:
                m_UnlockState = UnlockState::UNK_ERROR;
                break;
        }
    }

    threadEnd:
    m_IsRunning = false;
    m_HasConnection = false;
    SOCKET_CLOSE(m_ClientSocket);
}
