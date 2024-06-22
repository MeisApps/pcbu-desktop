#include "BTUnlockClient.h"

#include "platform/BluetoothHelper.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2bth.h>

#define AF_BLUETOOTH AF_BTH
#define BTPROTO_RFCOMM BTHPROTO_RFCOMM
#define read(x,y,z) recv(x, (char*)y, z, 0)
#define write(x,y,z) send(x, y, z, 0)
#define SOCKET_INVALID INVALID_SOCKET
#define SAFE_CLOSE(x) if(x != (SOCKET)-1) { closesocket(x); x = (SOCKET)-1; }
#endif
#ifdef LINUX
#include <unistd.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <sys/socket.h>

#define SOCKET_INVALID (-1)
#define SAFE_CLOSE(x) if(x != -1) { close(x); x = -1; }
#endif
#ifdef APPLE
#include <unistd.h>
#include <sys/socket.h>
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

#ifdef _WIN32
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

#ifndef APPLE
    if(m_ClientSocket != -1 && m_HasConnection)
        write(m_ClientSocket, "CLOSE", 5);

    m_IsRunning = false;
    m_HasConnection = false;
    SAFE_CLOSE(m_ClientSocket);
    if(m_AcceptThread.joinable())
        m_AcceptThread.join();
#else
#warning Not implemented on Apple.
#endif
}

std::vector<uint8_t> BTUnlockClient::ReadPacket() const {
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

void BTUnlockClient::WritePacket(const std::vector<uint8_t>& data) const {
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

void BTUnlockClient::ConnectThread() {
#ifdef LINUX
    // 62182bf7-97c8-45f9-aa2c-53c5f2008bdf
    static uint8_t CHANNEL_UUID[16] = { 0x62, 0x18, 0x2b, 0xf7, 0x97, 0xc8,
                                        0x45, 0xf9, 0xaa, 0x2c, 0x53, 0xc5, 0xf2, 0x00, 0x8b, 0xdf };

    m_Channel = BluetoothHelper::FindSDPChannel(m_DeviceAddress, CHANNEL_UUID);
    if (m_Channel == -1) {
        spdlog::error("Bluetooth sdp channel failed.");
        m_IsRunning = false;
        m_UnlockState = UnlockState::CONNECT_ERROR;
        return;
    }
#endif
#ifndef APPLE
    m_ClientSocket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if(m_ClientSocket == SOCKET_INVALID) {
        spdlog::error("Bluetooth socket() failed.");
        m_IsRunning = false;
        m_UnlockState = UnlockState::UNK_ERROR;
        return;
    }
#else
#warning Not implemented on Apple.
#endif

#ifdef _WIN32
    GUID guid = { 0x62182bf7, 0x97c8, 0x45f9, { 0xaa, 0x2c, 0x53, 0xc5, 0xf2, 0x00, 0x8b, 0xdf } };
    BTH_ADDR addr;
    BluetoothHelper::str2ba(m_DeviceAddress.c_str(), &addr);

    SOCKADDR_BTH address{};
    address.addressFamily = AF_BTH;
    address.serviceClassId = guid;
    address.btAddr = addr;
#endif
#ifdef LINUX
    struct sockaddr_rc address = { 0 };
    address.rc_family = AF_BLUETOOTH;
    address.rc_channel = m_Channel;
    str2ba(m_DeviceAddress.c_str(), &address.rc_bdaddr);
#endif

#ifndef APPLE
    int result = connect(m_ClientSocket, (struct sockaddr*)&address, sizeof(address));
    if(result == 0) {
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
        spdlog::error("Bluetooth connect() failed.");
        m_UnlockState = UnlockState::CONNECT_ERROR;
    }

    m_IsRunning = false;
    m_HasConnection = false;
    SAFE_CLOSE(m_ClientSocket);
#else
#warning Not implemented on Apple.
#endif
}
