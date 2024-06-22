#include "TCPUnlockClient.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <Ws2tcpip.h>

#define read(x,y,z) recv(x, (char*)y, z, 0)
#define write(x,y,z) send(x, y, z, 0)
#define SOCKET_INVALID INVALID_SOCKET
#define SAFE_CLOSE(x) if(x != (SOCKET)-1) { closesocket(x); x = (SOCKET)-1; }
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SOCKET_INVALID (-1)
#define SAFE_CLOSE(x) if(x != -1) { close(x); x = -1; }
#endif

TCPUnlockClient::TCPUnlockClient(const std::string& ipAddress, int port, const PairedDevice& device)
    : BaseUnlockServer(device) {
    m_IP = ipAddress;
    m_Port = port;
    m_ClientSocket = (SOCKET)-1;
    m_IsRunning = false;
}

bool TCPUnlockClient::Start() {
    if(m_IsRunning)
        return true;

#ifdef _WIN32
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

    if(m_ClientSocket != -1 && m_HasConnection)
        write(m_ClientSocket, "CLOSE", 5);

    m_IsRunning = false;
    m_HasConnection = false;
    SAFE_CLOSE(m_ClientSocket);
    if(m_AcceptThread.joinable())
        m_AcceptThread.join();
}

std::vector<uint8_t> TCPUnlockClient::ReadPacket() const {
    std::vector<uint8_t> lenBuffer{};
    lenBuffer.resize(sizeof(uint16_t));
    uint16_t lenBytesRead = 0;
    while (lenBytesRead < sizeof(uint16_t)) {
        int result = (int)read(m_ClientSocket, lenBuffer.data() + lenBytesRead, sizeof(uint16_t) - lenBytesRead);
        if (result <= 0) {
            spdlog::error("Reading length failed.");
            return {};
        }
        lenBytesRead += result;
    }

    uint16_t packetSize{};
    std::memcpy(&packetSize, lenBuffer.data(), sizeof(uint16_t));
    packetSize = ntohs(packetSize);
    if(packetSize == 0) {
        spdlog::error("Empty packet received.");
        return {};
    }

    std::vector<uint8_t> buffer{};
    buffer.resize(packetSize);
    uint16_t bytesRead = 0;
    while (bytesRead < packetSize) {
        int result = (int)read(m_ClientSocket, buffer.data() + bytesRead, packetSize - bytesRead);
        if (result <= 0) {
            spdlog::error("Reading data failed. (Len={})", packetSize);
            return {};
        }
        bytesRead += result;
    }
    return buffer;
}

void TCPUnlockClient::WritePacket(const std::vector<uint8_t>& data) const {
    uint16_t packetSize = htons(static_cast<uint16_t>(data.size()));
    int bytesWritten = 0;
    while (bytesWritten < sizeof(uint16_t)) {
        int result = (int)write(m_ClientSocket, reinterpret_cast<const char*>(&packetSize) + bytesWritten, sizeof(uint16_t) - bytesWritten);
        if (result <= 0) {
            spdlog::error("Writing data len failed.");
            return;
        }
        bytesWritten += result;
    }
    bytesWritten = 0;
    while (bytesWritten < data.size()) {
        int result = (int)write(m_ClientSocket, reinterpret_cast<const char*>(data.data()) + bytesWritten, (int)data.size() - bytesWritten);
        if (result <= 0) {
            spdlog::error("Writing data failed. (Len={})", packetSize);
            return;
        }
        bytesWritten += result;
    }
}

void TCPUnlockClient::ConnectThread() {
    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((u_short)m_Port);

    if (inet_pton(AF_INET, m_IP.c_str(), &serv_addr.sin_addr) <= 0) {
        spdlog::error("Invalid IP address.");
        m_IsRunning = false;
        m_UnlockState = UnlockState::UNK_ERROR;
        return;
    }

    if ((m_ClientSocket = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_INVALID) {
        spdlog::error("socket() failed.");
        m_IsRunning = false;
        m_UnlockState = UnlockState::UNK_ERROR;
        return;
    }

    int result = connect(m_ClientSocket, reinterpret_cast<struct sockaddr *>(&serv_addr),sizeof(serv_addr));
    if (result == 0) {
        m_HasConnection = true;
        auto serverDataStr = GetUnlockInfoPacket();
        if(serverDataStr.empty()) {
            m_IsRunning = false;
            m_UnlockState = UnlockState::UNK_ERROR;
            SAFE_CLOSE(m_ClientSocket);
            return;
        }

        WritePacket({serverDataStr.begin(), serverDataStr.end()});
        auto response = ReadPacket();
        OnResponseReceived(response.data(), response.size());
    } else {
        spdlog::error("connect() failed.");
        m_UnlockState = UnlockState::CONNECT_ERROR;
    }

    m_IsRunning = false;
    m_HasConnection = false;
    SAFE_CLOSE(m_ClientSocket);
}
