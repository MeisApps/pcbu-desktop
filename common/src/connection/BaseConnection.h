#ifndef PCBU_DESKTOP_BASECONNECTION_H
#define PCBU_DESKTOP_BASECONNECTION_H

#include <cstdint>
#include <vector>

#ifdef WINDOWS
typedef unsigned long long SOCKET;
#else
#define SOCKET int
#endif

enum class PacketError {
    UNKNOWN,
    NONE,
    CLOSED_CONNECTION,
    TIMEOUT
};

struct Packet {
    PacketError error{};
    std::vector<uint8_t> data{};
};

class BaseConnection {
protected:
    BaseConnection() = default;

    static bool SetSocketBlocking(SOCKET socket, bool isBlocking);
    static bool SetSocketRWTimeout(SOCKET socket, uint32_t secs);

    static Packet ReadPacket(SOCKET socket);
    static PacketError WritePacket(SOCKET socket, const std::vector<uint8_t>& data);

private:
    static PacketError GetPacketError(int result, int error);
};

#endif //PCBU_DESKTOP_BASECONNECTION_H
