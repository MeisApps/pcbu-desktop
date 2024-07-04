#ifndef PCBU_DESKTOP_SOCKETDEFS_H
#define PCBU_DESKTOP_SOCKETDEFS_H

#ifdef WINDOWS
#include <WinSock2.h>

#define read(x, y, z) recv(x, (char*)y, z, 0)
#define write(x, y, z) send(x, y, z, 0)

#define SOCKET_INVALID INVALID_SOCKET
#define SOCKET_IN_PROGRESS WSAEWOULDBLOCK
#define SOCKET_LAST_ERROR WSAGetLastError()
#define SOCKET_CLOSE(x) if(x != SOCKET_INVALID) { closesocket(x); x = SOCKET_INVALID; }
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/fcntl.h>

#define SOCKET_INVALID (-1)
#define SOCKET_IN_PROGRESS EINPROGRESS
#define SOCKET_LAST_ERROR errno
#define SOCKET_CLOSE(x) if(x != SOCKET_INVALID) { close(x); x = SOCKET_INVALID; }
#endif

#endif //PCBU_DESKTOP_SOCKETDEFS_H
