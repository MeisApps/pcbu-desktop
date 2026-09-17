#ifndef PCBU_DESKTOP_BASECONNECTION_H
#define PCBU_DESKTOP_BASECONNECTION_H

#include <cstdint>
#include <utility>
#include <vector>

#include "connection/stream/ConnectionStream.h"

#ifdef WINDOWS
typedef unsigned long long SOCKET;
#else
#define SOCKET int
#endif

struct Packet {
  PacketError error{};
  uint16_t id{};
  std::vector<uint8_t> data{};
};

class BaseConnection {
public:
  virtual ~BaseConnection() = default;

protected:
  BaseConnection() = default;

  static bool SetSocketBlocking(SOCKET socket, bool isBlocking);
  static bool SetSocketRWTimeout(SOCKET socket, uint32_t secs);

  static Packet ReadPacket(ConnectionStream &stream);
  static PacketError WritePacket(ConnectionStream &stream, uint16_t packetId, const std::vector<uint8_t> &data);

private:
  static std::pair<PacketError, std::vector<uint8_t>> ReadData(ConnectionStream &stream, uint32_t size);
};

#endif // PCBU_DESKTOP_BASECONNECTION_H
