#include "TCPUnlockServer.h"

#include "../SocketDefs.h"
#include "handler/UnlockState.h"
#include "storage/AppSettings.h"
#include "storage/PairedDevicesStorage.h"

#ifdef WINDOWS
#include <Ws2tcpip.h>
#else
#include <netinet/tcp.h>
#endif

constexpr int MAX_CLIENTS = 10;

TCPUnlockServer::TCPUnlockServer() : BaseUnlockConnection() {
  m_ServerSocket = SOCKET_INVALID;
}

bool TCPUnlockServer::IsServer() {
  return true;
}

bool TCPUnlockServer::Start() {
  if(m_IsRunning)
    return true;

  WSA_STARTUP
  m_IsRunning = true;
  m_AcceptThread = std::thread(&TCPUnlockServer::AcceptThread, this);
  return true;
}

void TCPUnlockServer::Stop() {
  if(!m_IsRunning)
    return;

  m_IsRunning = false;
  m_HasConnection = false;
  SOCKET_CLOSE(m_ServerSocket);
  if(m_AcceptThread.joinable())
    m_AcceptThread.join();
}

void TCPUnlockServer::AcceptThread() {
  struct sockaddr_in address {};
  socklen_t addrLen = sizeof(address);
  auto settings = AppSettings::Get();
  auto clientSockets = std::vector<SOCKET>();
  auto clientThreads = std::vector<std::thread>();
  spdlog::info("Starting TCP server...");

  if((m_ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == SOCKET_INVALID) {
    spdlog::error("socket() failed. (Code={})", SOCKET_LAST_ERROR);
    m_IsRunning = false;
    m_UnlockState = UnlockState::UNK_ERROR;
    return;
  }

  int opt = 1;
  if(setsockopt(m_ServerSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&opt), sizeof(opt))) {
    spdlog::error("setsockopt(SO_REUSEADDR) failed. (Code={})", SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::UNK_ERROR;
    goto threadEnd;
  }
  if(setsockopt(m_ServerSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&opt), sizeof(opt))) {
    spdlog::error("setsockopt(TCP_NODELAY) failed. (Code={})", SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::UNK_ERROR;
    goto threadEnd;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(settings.unlockServerPort);
  if(bind(m_ServerSocket, reinterpret_cast<struct sockaddr *>(&address), sizeof(address)) < 0) {
    spdlog::error("bind() failed. (Code={})", SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::PORT_ERROR;
    goto threadEnd;
  }
  if(listen(m_ServerSocket, 3) < 0) {
    spdlog::error("listen() failed. (Code={})", SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::UNK_ERROR;
    goto threadEnd;
  }
  if(!SetSocketBlocking(m_ServerSocket, false)) {
    spdlog::error("Failed setting server socket to non-blocking mode. (Code={})", SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::UNK_ERROR;
    goto threadEnd;
  }

  spdlog::info("TCP server started on port '{}'.", settings.unlockServerPort);
  while(m_IsRunning) {
    if(clientSockets.size() >= MAX_CLIENTS) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }
    SOCKET clientSocket;
    if((clientSocket = accept(m_ServerSocket, reinterpret_cast<struct sockaddr *>(&address), (socklen_t *)&addrLen)) == SOCKET_INVALID) {
      auto err = SOCKET_LAST_ERROR;
      if(err == SOCKET_ERROR_TRY_AGAIN || err == SOCKET_ERROR_WOULD_BLOCK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      }
      if(err != SOCKET_ERROR_CONNECT_ABORTED)
        spdlog::error("accept() failed. (Code={})", err);
      break;
    }
    if(!SetSocketBlocking(clientSocket, false)) {
      spdlog::error("Failed setting client socket to non-blocking mode. (Code={})", SOCKET_LAST_ERROR);
      m_UnlockState = UnlockState::UNK_ERROR;
      break;
    }
    clientSockets.emplace_back(clientSocket);
    clientThreads.emplace_back(&TCPUnlockServer::ClientThread, this, clientSocket);
  }

threadEnd:
  m_IsRunning = false;
  for(auto clientSocket : clientSockets) {
    SOCKET_CLOSE(clientSocket);
  }
  SOCKET_CLOSE(m_ServerSocket);
  for(auto &thread : clientThreads)
    if(thread.joinable())
      thread.join();
  m_HasConnection = false;
  spdlog::info("TCP server stopped.");
}

void TCPUnlockServer::ClientThread(SOCKET clientSocket) {
  spdlog::info("TCP client connected.");
  m_HasConnection = true;
  ++m_NumConnections;
  int opt = 1;
  if(setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&opt), sizeof(opt))) {
    spdlog::error("setsockopt(TCP_NODELAY) failed. (Code={})", SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::UNK_ERROR;
  } else {
    PerformAuthFlow(clientSocket, true);
  }
  --m_NumConnections;
  m_HasConnection = m_NumConnections > 0;
  SOCKET_CLOSE(clientSocket);
  spdlog::info("TCP client closed.");
}
