#include "SocketStream.h"

#include <cstring>
#include <spdlog/spdlog.h>

#include "connection/SocketDefs.h"

#ifndef WINDOWS
#include <poll.h>
#endif

constexpr int READ_WAIT_SLICE_MS = 100;

SocketStream::SocketStream(SOCKET socket, const std::atomic<bool> *isRunning, uint32_t idleTimeoutSecs)
    : m_Socket(socket), m_IsRunning(isRunning), m_IdleTimeout(idleTimeoutSecs), m_LastActivity(std::chrono::steady_clock::now()) {}

StreamResult SocketStream::Read(uint8_t *buffer, size_t length) {
  if(m_IsRunning != nullptr) {
    if(!m_IsRunning->load())
      return {0, PacketError::CLOSED_CONNECTION};
    if(WaitReadable(READ_WAIT_SLICE_MS) == 0) {
      if(IsIdleTimedOut())
        return {0, PacketError::TIMEOUT};
      return {0, PacketError::NONE};
    }
  }
  int result = SocketRead(m_Socket, buffer, length);
  if(result > 0) {
    m_LastActivity = std::chrono::steady_clock::now();
    return {result, PacketError::NONE};
  }
  auto error = GetPacketError(result, SOCKET_LAST_ERROR);
  if(error == PacketError::NONE && IsIdleTimedOut())
    return {0, PacketError::TIMEOUT};
  return {0, error};
}

StreamResult SocketStream::WriteRaw(const uint8_t *buffer, size_t length) {
  if(m_IsRunning != nullptr && !m_IsRunning->load())
    return {0, PacketError::CLOSED_CONNECTION};
  int result = SocketWrite(m_Socket, buffer, length);
  if(result > 0) {
    m_LastActivity = std::chrono::steady_clock::now();
    return {result, PacketError::NONE};
  }
  auto error = GetPacketError(result, SOCKET_LAST_ERROR);
  if(error == PacketError::NONE && IsIdleTimedOut())
    return {0, PacketError::TIMEOUT};
  return {0, error};
}

void SocketStream::Close() {
  SOCKET_CLOSE(m_Socket);
}

SOCKET SocketStream::GetSocket() const {
  return m_Socket;
}

bool SocketStream::IsIdleTimedOut() const {
  if(m_IdleTimeout.count() == 0)
    return false;
  if(std::chrono::steady_clock::now() - m_LastActivity < m_IdleTimeout)
    return false;
  spdlog::error("Socket idle timeout reached. (Timeout={}s)", m_IdleTimeout.count());
  return true;
}

int SocketStream::WaitReadable(int timeoutMs) const {
#ifdef WINDOWS
  fd_set readSet{};
  FD_ZERO(&readSet);
  FD_SET(m_Socket, &readSet);
  struct timeval timeout{};
  timeout.tv_sec = timeoutMs / 1000;
  timeout.tv_usec = (timeoutMs % 1000) * 1000;
  return select(0, &readSet, nullptr, nullptr, &timeout);
#else
  struct pollfd pfd{};
  pfd.fd = m_Socket;
  pfd.events = POLLIN;
  return poll(&pfd, 1, timeoutMs);
#endif
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
