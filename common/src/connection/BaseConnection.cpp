#include "BaseConnection.h"

#include <spdlog/spdlog.h>

#include "SocketDefs.h"

bool BaseConnection::SetSocketBlocking(SOCKET socket, bool isBlocking) {
#ifdef WINDOWS
    u_long mode = isBlocking ? 0 : 1;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    int oldFlags = fcntl(socket, F_GETFL, 0);
    if (oldFlags == -1)
        return false;
    auto flags = isBlocking ? (oldFlags & ~O_NONBLOCK) : (oldFlags | O_NONBLOCK);
    return fcntl(socket, F_SETFL, flags) != -1;
#endif
}

bool BaseConnection::SetSocketRWTimeout(SOCKET socket, uint32_t secs) {
#ifdef WINDOWS
    auto timeoutVal = (DWORD)secs * 1000;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeoutVal), sizeof(timeoutVal)) < 0 ||
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeoutVal), sizeof(timeoutVal)) < 0) {
#else
    struct timeval timeoutVal{};
    timeoutVal.tv_sec = secs;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeoutVal, sizeof(timeoutVal)) < 0 ||
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeoutVal, sizeof(timeoutVal)) < 0) {
#endif
        return false;
    }
    return true;
}

Packet BaseConnection::ReadPacket(SOCKET socket) {
    std::vector<uint8_t> lenBuffer{};
    lenBuffer.resize(sizeof(uint16_t));
    uint16_t lenBytesRead = 0;
    while (lenBytesRead < sizeof(uint16_t)) {
        int result = (int)read(socket, lenBuffer.data() + lenBytesRead, sizeof(uint16_t) - lenBytesRead);
        if (result <= 0) {
            auto error = GetPacketError(result, SOCKET_LAST_ERROR);
            if(error != PacketError::NONE) {
                spdlog::error("Reading packet length failed.");
                return {GetPacketError(result, SOCKET_LAST_ERROR)};
            }
        } else {
            lenBytesRead += result;
        }
    }

    uint16_t packetSize{};
    std::memcpy(&packetSize, lenBuffer.data(), sizeof(uint16_t));
    packetSize = ntohs(packetSize);
    if(packetSize == 0) {
        spdlog::error("Empty packet received.");
        return {PacketError::UNKNOWN};
    }

    std::vector<uint8_t> buffer{};
    buffer.resize(packetSize);
    uint16_t bytesRead = 0;
    while (bytesRead < packetSize) {
        int result = (int)read(socket, buffer.data() + bytesRead, packetSize - bytesRead);
        if (result <= 0) {
            auto error = GetPacketError(result, SOCKET_LAST_ERROR);
            if(error != PacketError::NONE) {
                spdlog::error("Reading packet data failed. (Len={})", packetSize);
                return {GetPacketError(result, SOCKET_LAST_ERROR)};
            }
        } else {
            bytesRead += result;
        }
    }
    return {PacketError::NONE, buffer};
}

PacketError BaseConnection::WritePacket(SOCKET socket, const std::vector<uint8_t>& data) {
    uint16_t packetSize = htons(static_cast<uint16_t>(data.size()));
    int bytesWritten = 0;
    while (bytesWritten < sizeof(uint16_t)) {
        int result = (int)write(socket, reinterpret_cast<const char*>(&packetSize) + bytesWritten, sizeof(uint16_t) - bytesWritten);
        if (result <= 0) {
            auto error = GetPacketError(result, SOCKET_LAST_ERROR);
            if(error != PacketError::NONE) {
                spdlog::error("Writing packet data length failed.");
                return error;
            }
        } else {
            bytesWritten += result;
        }
    }
    bytesWritten = 0;
    while (bytesWritten < data.size()) {
        int result = (int)write(socket, reinterpret_cast<const char*>(data.data()) + bytesWritten, (int)data.size() - bytesWritten);
        if (result <= 0) {
            auto error = GetPacketError(result, SOCKET_LAST_ERROR);
            if(error != PacketError::NONE) {
                spdlog::error("Writing packet data failed. (Len={})", packetSize);
                return error;
            }
        } else {
            bytesWritten += result;
        }
    }
    return PacketError::NONE;
}

PacketError BaseConnection::GetPacketError(int result, int error) {
    if(result == 0) {
        return PacketError::CLOSED_CONNECTION;
    } else {
        if(error == SOCKET_ERROR_WOULD_BLOCK || error == SOCKET_ERROR_IN_PROGRESS || error == SOCKET_ERROR_TRY_AGAIN)
            return PacketError::NONE;

        spdlog::error("Socket operation failed. (Code={}, Str={})", error, strerror(error));
        if(error == SOCKET_ERROR_CONNECT_REFUSED || error == SOCKET_ERROR_HOST_UNREACHABLE)
            return PacketError::CLOSED_CONNECTION;
        if(error == SOCKET_ERROR_TIMEOUT)
            return PacketError::TIMEOUT;
    }
    return PacketError::UNKNOWN;
}
