#include "BTUnlockServer.h"

#include "../SocketDefs.h"
#include "platform/BluetoothHelper.h"

#ifdef WINDOWS
#include <WS2tcpip.h>
#include <ws2bth.h>
#define AF_BLUETOOTH AF_BTH
#define BTPROTO_RFCOMM BTHPROTO_RFCOMM
#elif LINUX
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#endif

BTUnlockServer::BTUnlockServer(const PairedDevice &device)
    : BaseUnlockConnection(device) {
    m_ServerSocket = SOCKET_INVALID;
}

bool BTUnlockServer::Start() {
    if(m_IsRunning)
        return true;

    WSA_STARTUP
    m_IsRunning = true;
    m_AcceptThread = std::thread(&BTUnlockServer::AcceptThread, this);
    return true;
}

void BTUnlockServer::Stop() {
    if(!m_IsRunning)
        return;

    m_IsRunning = false;
    m_HasConnection = false;
    SOCKET_CLOSE(m_ServerSocket);
    if(m_AcceptThread.joinable())
        m_AcceptThread.join();
}

void BTUnlockServer::PerformAuthFlow(SOCKET socket) {
    BaseUnlockConnection::PerformAuthFlow(socket);
}

void BTUnlockServer::AcceptThread() {
    std::optional<SDPService> sdpService{};
    spdlog::info("Starting BT server...");
#ifdef WINDOWS
    SOCKADDR_BTH address{};
    SOCKADDR sockAddr{};
    address.addressFamily = AF_BTH;
    address.port = BT_PORT_ANY;
    auto sockAddrLen = (socklen_t)sizeof(sockAddr);
#elif LINUX
    struct sockaddr_rc address = { 0 };
    address.rc_family = AF_BLUETOOTH;
    address.rc_bdaddr = (bdaddr_t){{0, 0, 0, 0, 0, 0}};
    address.rc_channel = (uint8_t)27;
#endif
    auto addrLen = (socklen_t)sizeof(address);

    if((m_ServerSocket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM)) == SOCKET_INVALID) {
        spdlog::error("socket(AF_BLUETOOTH) failed. (Code={})", SOCKET_LAST_ERROR);
        m_IsRunning = false;
        m_UnlockState = UnlockState::UNK_ERROR;
        return;
    }

    int opt = 1;
    if (setsockopt(m_ServerSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&opt), sizeof(opt))) {
        spdlog::error("setsockopt(SO_REUSEADDR) failed. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }

    if (bind(m_ServerSocket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        spdlog::error("bind() failed. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }

#ifdef WINDOWS
    if(getsockname(m_ServerSocket, (SOCKADDR *)&sockAddr, &sockAddrLen) != 0) {
        spdlog::error("getsockname() failed. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }
    sdpService = BluetoothHelper::RegisterSDPService(sockAddr);
#else
    sdpService = BluetoothHelper::RegisterSDPService(address.rc_channel);
#endif
    if(!sdpService) {
        spdlog::error("RegisterSDPService() failed. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }

    if (listen(m_ServerSocket, 3) < 0) {
        spdlog::error("listen() failed. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }

    spdlog::info("Server started.");
    while (m_IsRunning) {
        SOCKET clientSocket;
        if ((clientSocket = accept(m_ServerSocket, (struct sockaddr *)&address, (socklen_t *)&addrLen)) == SOCKET_INVALID) {
            spdlog::error("accept() failed. (Code={})", SOCKET_LAST_ERROR);
            break;
        }
        if(!SetSocketBlocking(clientSocket, false)) {
            spdlog::error("Failed setting client socket to non-blocking mode. (Code={})", SOCKET_LAST_ERROR);
            m_UnlockState = UnlockState::UNK_ERROR;
            break;
        }
        spdlog::info("BT client connected.");
        m_ClientThreads.emplace_back(&BTUnlockServer::ClientThread, this, clientSocket);
    }

    threadEnd:
    m_IsRunning = false;
    m_HasConnection = false;
    if(!BluetoothHelper::CloseSDPService(sdpService.value()))
        spdlog::warn("CloseSDPService() failed. (Code={})", SOCKET_LAST_ERROR);
    SOCKET_CLOSE(m_ServerSocket);
    for(auto& thread : m_ClientThreads)
        if(thread.joinable())
            thread.join();
    m_ClientThreads.clear();
}

void BTUnlockServer::ClientThread(SOCKET clientSocket) {
    m_HasConnection = true;
    PerformAuthFlow(clientSocket);

    m_HasConnection = false;
    SOCKET_CLOSE(clientSocket);
    spdlog::info("BT Client closed.");
}
