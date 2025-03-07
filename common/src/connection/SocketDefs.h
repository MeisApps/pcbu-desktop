#ifndef PCBU_DESKTOP_SOCKETDEFS_H
#define PCBU_DESKTOP_SOCKETDEFS_H

#ifdef WINDOWS
#include <WinSock2.h>

#define read(x, y, z) recv(x, (char *)y, z, 0)
#define write(x, y, z) send(x, y, z, 0)

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
