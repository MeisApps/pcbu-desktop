#ifndef PCBU_DESKTOP_CONNECTIONSTREAM_H
#define PCBU_DESKTOP_CONNECTIONSTREAM_H

#include <cstddef>
#include <cstdint>
#include <vector>

enum class PacketError { UNKNOWN, NONE, CLOSED_CONNECTION, TIMEOUT, UNSUPPORTED_VERSION, PEER_UNAVAILABLE };

struct StreamResult {
  int bytes{};
  PacketError error{};
};

class ConnectionStream {
public:
  virtual ~ConnectionStream() = default;

  virtual StreamResult Read(uint8_t *buffer, size_t length) = 0;
  virtual StreamResult WriteRaw(const uint8_t *buffer, size_t length) = 0;
  virtual void Close() = 0;

  void Write(const uint8_t *buffer, size_t length);
  PacketError Flush();

private:
  std::vector<uint8_t> m_WriteBuffer{};
};

#endif // PCBU_DESKTOP_CONNECTIONSTREAM_H
