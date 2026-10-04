#include "TCPUnlockServer.h"

#include "connection/SocketDefs.h"
#include "connection/stream/SocketStream.h"
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

bool TCPUnlockServer::Start() {
  if(m_IsRunning)
    return true;

  WSA_STARTUP
  m_IsRunning = true;
  SetPhase(UnlockPhase::SERVER_WAITING);
  m_AcceptThread = std::thread([this]() {
    try {
      AcceptThread();
    } catch(const std::exception &ex) {
      spdlog::error("TCP server failed: {}", ex.what());
      m_UnlockState = UnlockState::UNK_ERROR;
      m_IsRunning = false;
    }
  });
  return true;
}

void TCPUnlockServer::Stop() {
  m_IsRunning = false;
  if(m_AcceptThread.joinable())
    m_AcceptThread.join();
}

void TCPUnlockServer::AcceptThread() {
  struct sockaddr_in address {};
  socklen_t addrLen = sizeof(address);
  auto settings = AppSettings::Get();
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
  if(listen(m_ServerSocket, MAX_CLIENTS) < 0) {
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
    if(m_NumConnections >= MAX_CLIENTS) {
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
      SOCKET_CLOSE(clientSocket);
      break;
    }
    try {
      clientThreads.emplace_back(&TCPUnlockServer::ClientThread, this, clientSocket, settings.clientSocketTimeout);
    } catch(const std::exception &ex) {
      spdlog::error("Failed starting TCP client thread: {}", ex.what());
      m_UnlockState = UnlockState::UNK_ERROR;
      SOCKET_CLOSE(clientSocket);
      break;
    }
  }

threadEnd:
  m_IsRunning = false;
  for(auto &thread : clientThreads)
    if(thread.joinable())
      thread.join();
  SOCKET_CLOSE(m_ServerSocket);
  spdlog::info("TCP server stopped.");
}

void TCPUnlockServer::ClientThread(SOCKET clientSocket, uint32_t idleTimeoutSecs) {
  spdlog::info("TCP client connected.");
  ++m_NumConnections;
  int opt = 1;
  if(setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&opt), sizeof(opt))) {
    spdlog::error("setsockopt(TCP_NODELAY) failed. (Code={})", SOCKET_LAST_ERROR);
  }
  try {
    SocketStream stream(clientSocket, &m_IsRunning, idleTimeoutSecs);
    PerformAuthFlow(stream, true);
  } catch(const std::exception &ex) {
    spdlog::error("TCP client failed: {}", ex.what());
    m_UnlockState = UnlockState::UNK_ERROR;
  }
  --m_NumConnections;
  if(PollResult() == UnlockState::UNKNOWN)
    SetPhase(m_NumConnections > 0 ? UnlockPhase::PHONE_UNLOCKING : UnlockPhase::SERVER_WAITING);
  SOCKET_CLOSE(clientSocket);
  spdlog::info("TCP client closed.");
}
