#ifndef PCBU_DESKTOP_SOCKETSTREAM_H
#define PCBU_DESKTOP_SOCKETSTREAM_H

#include <atomic>
#include <chrono>

#include "connection/BaseConnection.h"

class SocketStream : public ConnectionStream {
public:
  explicit SocketStream(SOCKET socket, const std::atomic<bool> *isRunning = nullptr, uint32_t idleTimeoutSecs = 0);

  StreamResult Read(uint8_t *buffer, size_t length) override;
  StreamResult WriteRaw(const uint8_t *buffer, size_t length) override;
  void Close() override;

  [[nodiscard]] SOCKET GetSocket() const;

private:
  [[nodiscard]] int WaitReadable(int timeoutMs) const;
  static PacketError GetPacketError(int result, int error);
  [[nodiscard]] bool IsIdleTimedOut() const;

  SOCKET m_Socket;
  const std::atomic<bool> *m_IsRunning{};
  std::chrono::seconds m_IdleTimeout{};
  std::chrono::steady_clock::time_point m_LastActivity{};
};

#endif // PCBU_DESKTOP_SOCKETSTREAM_H
