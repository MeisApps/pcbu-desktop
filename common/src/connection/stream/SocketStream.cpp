#include "SocketStream.h"

#include <cstring>
#include <spdlog/spdlog.h>

#include "connection/SocketDefs.h"

SocketStream::SocketStream(SOCKET socket) : m_Socket(socket) {}

StreamResult SocketStream::Read(uint8_t *buffer, size_t length) {
  int result = SocketRead(m_Socket, buffer, length);
  if(result > 0)
    return {result, PacketError::NONE};
  return {0, GetPacketError(result, SOCKET_LAST_ERROR)};
}

StreamResult SocketStream::WriteRaw(const uint8_t *buffer, size_t length) {
  int result = SocketWrite(m_Socket, buffer, length);
  if(result > 0)
    return {result, PacketError::NONE};
  return {0, GetPacketError(result, SOCKET_LAST_ERROR)};
}

void SocketStream::Close() {
  SOCKET_CLOSE(m_Socket);
}

SOCKET SocketStream::GetSocket() const {
  return m_Socket;
}

PacketError SocketStream::GetPacketError(int result, int error) {
  if(result == 0)
    return PacketError::CLOSED_CONNECTION;
  if(error == SOCKET_ERROR_WOULD_BLOCK || error == SOCKET_ERROR_IN_PROGRESS || error == SOCKET_ERROR_TRY_AGAIN)
    return PacketError::NONE;

  spdlog::error("Socket operation failed. (Code={}, Str={})", error, strerror(error));
  if(error == SOCKET_ERROR_CONNECT_REFUSED || error == SOCKET_ERROR_HOST_UNREACHABLE || error == SOCKET_ERROR_CONNECT_ABORTED ||
     error == SOCKET_ERROR_CONNECT_RESET || error == SOCKET_ERROR_NET_UNREACHABLE)
    return PacketError::CLOSED_CONNECTION;
  if(error == SOCKET_ERROR_TIMEOUT)
    return PacketError::TIMEOUT;
#ifdef WINDOWS
  if(error == WSAESHUTDOWN || error == WSAENOTSOCK)
    return PacketError::CLOSED_CONNECTION;
#else
  if(error == EPIPE)
    return PacketError::CLOSED_CONNECTION;
#endif
  return PacketError::UNKNOWN;
}
