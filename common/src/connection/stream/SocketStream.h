#ifndef PCBU_DESKTOP_SOCKETSTREAM_H
#define PCBU_DESKTOP_SOCKETSTREAM_H

#include "connection/BaseConnection.h"

class SocketStream : public ConnectionStream {
public:
  explicit SocketStream(SOCKET socket);

  StreamResult Read(uint8_t *buffer, size_t length) override;
  StreamResult WriteRaw(const uint8_t *buffer, size_t length) override;
  void Close() override;

  [[nodiscard]] SOCKET GetSocket() const;

private:
  static PacketError GetPacketError(int result, int error);

  SOCKET m_Socket;
};

#endif // PCBU_DESKTOP_SOCKETSTREAM_H
