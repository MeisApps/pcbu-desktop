#ifndef PCBU_DESKTOP_SOCKETDEFS_H
#define PCBU_DESKTOP_SOCKETDEFS_H

#include <cstddef>

#ifdef WINDOWS
#include <WinSock2.h>

inline int SocketRead(SOCKET sock, void *buffer, size_t length) {
  return recv(sock, static_cast<char *>(buffer), static_cast<int>(length), 0);
}

inline int SocketWrite(SOCKET sock, const void *buffer, size_t length) {
  return send(sock, static_cast<const char *>(buffer), static_cast<int>(length), 0);
}

#define WSA_STARTUP                                                                                                                                  \
  WSADATA wsa{};                                                                                                                                     \
  if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {                                                                                                        \
    spdlog::error("WSAStartup failed.");                                                                                                             \
    return false;                                                                                                                                    \
  }

#define SOCKET_INVALID INVALID_SOCKET
#define SOCKET_ERROR_TRY_AGAIN WSAEWOULDBLOCK // WOULDBLOCK on Windows
#define SOCKET_ERROR_IN_PROGRESS WSAEINPROGRESS
#define SOCKET_ERROR_WOULD_BLOCK WSAEWOULDBLOCK
#define SOCKET_ERROR_TIMEOUT WSAETIMEDOUT
#define SOCKET_ERROR_CONNECT_REFUSED WSAECONNREFUSED
#define SOCKET_ERROR_CONNECT_ABORTED WSAECONNABORTED
#define SOCKET_ERROR_CONNECT_RESET WSAECONNRESET
#define SOCKET_ERROR_HOST_UNREACHABLE WSAEHOSTUNREACH
#define SOCKET_ERROR_NET_UNREACHABLE WSAENETUNREACH
#define SOCKET_LAST_ERROR WSAGetLastError()
#define SOCKET_CLOSE(x)                                                                                                                              \
  if(x != SOCKET_INVALID) {                                                                                                                          \
    closesocket(x);                                                                                                                                  \
    x = SOCKET_INVALID;                                                                                                                              \
  }
#else
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

inline int SocketRead(int sock, void *buffer, size_t length) {
  return static_cast<int>(::read(sock, buffer, length));
}

inline int SocketWrite(int sock, const void *buffer, size_t length) {
  return static_cast<int>(::write(sock, buffer, length));
}

#define WSA_STARTUP

#define SOCKET_INVALID (-1)
#define SOCKET_ERROR_TRY_AGAIN EAGAIN
#define SOCKET_ERROR_IN_PROGRESS EINPROGRESS
#define SOCKET_ERROR_WOULD_BLOCK EWOULDBLOCK
#define SOCKET_ERROR_TIMEOUT ETIMEDOUT
#define SOCKET_ERROR_CONNECT_REFUSED ECONNREFUSED
#define SOCKET_ERROR_CONNECT_ABORTED ECONNABORTED
#define SOCKET_ERROR_CONNECT_RESET ECONNRESET
#define SOCKET_ERROR_HOST_UNREACHABLE EHOSTUNREACH
#define SOCKET_ERROR_NET_UNREACHABLE ENETUNREACH
#define SOCKET_LAST_ERROR errno
#define SOCKET_CLOSE(x)                                                                                                                              \
  if(x != SOCKET_INVALID) {                                                                                                                          \
    close(x);                                                                                                                                        \
    x = SOCKET_INVALID;                                                                                                                              \
  }
#endif

#endif // PCBU_DESKTOP_SOCKETDEFS_H
