#ifndef PCBU_DESKTOP_BASECONNECTION_H
#define PCBU_DESKTOP_BASECONNECTION_H

#include <cstdint>
#include <vector>

#ifdef WINDOWS
typedef unsigned long long SOCKET;
#else
#define SOCKET int
#endif

enum class PacketError { UNKNOWN, NONE, CLOSED_CONNECTION, TIMEOUT };

struct Packet {
  PacketError error{};
  uint16_t id{};
  std::vector<uint8_t> data{};
};

class BaseConnection {
public:
  virtual ~BaseConnection() = default;
  virtual bool IsServer();

protected:
  BaseConnection() = default;

  static bool SetSocketBlocking(SOCKET socket, bool isBlocking);
  static bool SetSocketRWTimeout(SOCKET socket, uint32_t secs);

  static Packet ReadPacket(SOCKET socket);
  static PacketError WritePacket(SOCKET socket, uint16_t packetId, const std::vector<uint8_t> &data);

private:
  static std::pair<PacketError, std::vector<uint8_t>> ReadData(SOCKET socket, uint32_t size);
  static PacketError WriteData(SOCKET socket, const char *data, uint32_t size);

  static PacketError GetPacketError(int result, int error);
};

#endif // PCBU_DESKTOP_BASECONNECTION_H
