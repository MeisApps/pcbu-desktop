#include "TCPUnlockServer.h"

#include "../SocketDefs.h"
#include "storage/AppSettings.h"

TCPUnlockServer::TCPUnlockServer(const PairedDevice &device)
    : BaseUnlockConnection(device) {
    m_ServerSocket = SOCKET_INVALID;
}

bool TCPUnlockServer::Start() {
    if(m_IsRunning)
        return true;

    m_IsRunning = true;
    m_AcceptThread = std::thread(&TCPUnlockServer::AcceptThread, this);
    return true;
}

void TCPUnlockServer::Stop() {
    if(!m_IsRunning)
        return;

    m_IsRunning = false;
    m_HasConnection = false;
    SOCKET_CLOSE(m_ServerSocket);
    if(m_AcceptThread.joinable())
        m_AcceptThread.join();
}

void TCPUnlockServer::PerformAuthFlow(int socket) {
    BaseUnlockConnection::PerformAuthFlow(socket);
}

void TCPUnlockServer::AcceptThread() {
    struct sockaddr_in address{};
    socklen_t addrLen = sizeof(address);
    auto settings = AppSettings::Get();
    spdlog::info("Starting TCP server...");

    if ((m_ServerSocket = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_INVALID) {
        spdlog::error("socket() failed. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        return;
    }

    int opt = 1;
    if (setsockopt(m_ServerSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        spdlog::error("setsockopt(SO_REUSEADDR) failed. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(settings.unlockServerPort);
    if (bind(m_ServerSocket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        spdlog::error("bind() failed. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }
    if (listen(m_ServerSocket, 3) < 0) {
        spdlog::error("listen() failed. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }

    spdlog::info("Server started on port '{}'.", settings.unlockServerPort);
    while (m_IsRunning) {
        SOCKET clientSocket;
        if ((clientSocket = accept(m_ServerSocket, (struct sockaddr *)&address, (socklen_t *)&addrLen)) < 0) {
            spdlog::error("accept() failed. (Code={})", SOCKET_LAST_ERROR);
            break;
        }
        if(!SetSocketBlocking(clientSocket, false)) {
            spdlog::error("Failed setting client socket to non-blocking mode. (Code={})", SOCKET_LAST_ERROR);
            m_UnlockState = UnlockState::UNK_ERROR;
            break;
        }
        spdlog::info("TCP client connected.");
        m_ClientThreads.emplace_back(&TCPUnlockServer::ClientThread, this, clientSocket);
    }

    threadEnd:
    m_IsRunning = false;
    m_HasConnection = false;
    SOCKET_CLOSE(m_ServerSocket);
    for(auto& thread : m_ClientThreads)
        if(thread.joinable())
            thread.join();
    m_ClientThreads.clear();
}

void TCPUnlockServer::ClientThread(SOCKET clientSocket) {
    m_HasConnection = true;
    PerformAuthFlow(clientSocket);

    m_HasConnection = false;
    SOCKET_CLOSE(clientSocket);
    spdlog::info("TCP Client closed.");
}
