#include "BTUnlockClient.h"

#include "connection/SocketDefs.h"
#include "platform/BluetoothHelper.h"
#include "storage/AppSettings.h"

#ifdef WINDOWS
#include <ws2bth.h>

#define AF_BLUETOOTH AF_BTH
#define BTPROTO_RFCOMM BTHPROTO_RFCOMM
#elif LINUX
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#endif

BTUnlockClient::BTUnlockClient(const std::string& deviceAddress, const PairedDevice& device)
    : BaseUnlockServer(device) {
    m_DeviceAddress = deviceAddress;
    m_Channel = -1;
    m_ClientSocket = (SOCKET)-1;
    m_IsRunning = false;
}

bool BTUnlockClient::Start() {
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
    m_AcceptThread = std::thread(&BTUnlockClient::ConnectThread, this);
    return true;
}

void BTUnlockClient::Stop() {
    if(!m_IsRunning)
        return;

    if(m_ClientSocket != -1 && m_HasConnection)
        write(m_ClientSocket, "CLOSE", 5);

    m_IsRunning = false;
    m_HasConnection = false;
    SOCKET_CLOSE(m_ClientSocket);
    if(m_AcceptThread.joinable())
        m_AcceptThread.join();
}

void BTUnlockClient::ConnectThread() {
    std::string serverDataStr{};
    Packet responsePacket{};
    uint32_t numRetries{};
    auto settings = AppSettings::Get();

#ifdef WINDOWS
    GUID guid = { 0x62182bf7, 0x97c8, 0x45f9, { 0xaa, 0x2c, 0x53, 0xc5, 0xf2, 0x00, 0x8b, 0xdf } };
    BTH_ADDR addr;
    BluetoothHelper::str2ba(m_DeviceAddress.c_str(), &addr);

    SOCKADDR_BTH address{};
    address.addressFamily = AF_BTH;
    address.serviceClassId = guid;
    address.btAddr = addr;
#elif LINUX
    // 62182bf7-97c8-45f9-aa2c-53c5f2008bdf
    static uint8_t CHANNEL_UUID[16] = { 0x62, 0x18, 0x2b, 0xf7, 0x97, 0xc8,
                                        0x45, 0xf9, 0xaa, 0x2c, 0x53, 0xc5, 0xf2, 0x00, 0x8b, 0xdf };

    m_Channel = BluetoothHelper::FindSDPChannel(m_DeviceAddress, CHANNEL_UUID);
    if (m_Channel == -1) {
        spdlog::error("Bluetooth FindSDPChannel failed.");
        m_IsRunning = false;
        m_UnlockState = UnlockState::CONNECT_ERROR;
        return;
    }

    struct sockaddr_rc address = { 0 };
    address.rc_family = AF_BLUETOOTH;
    address.rc_channel = m_Channel;
    str2ba(m_DeviceAddress.c_str(), &address.rc_bdaddr);
#endif

    socketStart:
    if((m_ClientSocket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM)) == SOCKET_INVALID) {
        spdlog::error("Bluetooth socket() failed. (Code={})", SOCKET_LAST_ERROR);
        m_IsRunning = false;
        m_UnlockState = UnlockState::UNK_ERROR;
        return;
    }

    fd_set fdSet{};
    FD_SET(m_ClientSocket, &fdSet);
    struct timeval timeout{};
    timeout.tv_sec = (long)settings.clientSocketTimeout;

#ifdef WINDOWS
    auto timeoutMs = (DWORD)(timeout.tv_sec * 1000);
    if (setsockopt(m_ClientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs)) < 0 ||
        setsockopt(m_ClientSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs)) < 0) {
        spdlog::error("setsockopt() for timeout failed. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }
#else
    if (setsockopt(m_ClientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(m_ClientSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
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

    if(connect(m_ClientSocket, reinterpret_cast<struct sockaddr *>(&address), sizeof(address)) < 0) {
        auto error = SOCKET_LAST_ERROR;
        if(error != SOCKET_ERROR_IN_PROGRESS && error != SOCKET_ERROR_WOULD_BLOCK) {
            spdlog::error("connect() failed. (Code={})", error);
            m_UnlockState = UnlockState::CONNECT_ERROR;
            goto threadEnd;
        }
    }
    if(select((int)m_ClientSocket + 1, nullptr, &fdSet, nullptr, &timeout) <= 0) {
        spdlog::error("select() timed out or failed. (Code={}, Retry={})", SOCKET_LAST_ERROR, numRetries);
        if(numRetries < settings.clientConnectRetries && m_IsRunning) {
            SOCKET_CLOSE(m_ClientSocket);
            numRetries++;
            goto socketStart;
        }
        m_UnlockState = UnlockState::CONNECT_ERROR;
        goto threadEnd;
    }

    if(!SetSocketBlocking(m_ClientSocket, true)) {
        spdlog::error("Failed setting socket to blocking mode. (Code={})", SOCKET_LAST_ERROR);
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }

    m_HasConnection = true;
    serverDataStr = GetUnlockInfoPacket();
    if(serverDataStr.empty()) {
        m_UnlockState = UnlockState::UNK_ERROR;
        goto threadEnd;
    }
    WritePacket(m_ClientSocket, {serverDataStr.begin(), serverDataStr.end()});
    responsePacket = ReadPacket(m_ClientSocket);
    OnResponseReceived(responsePacket);

    threadEnd:
    m_IsRunning = false;
    m_HasConnection = false;
    SOCKET_CLOSE(m_ClientSocket);
}
