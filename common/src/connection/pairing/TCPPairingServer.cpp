#include "TCPPairingServer.h"

#include "connection/stream/SocketStream.h"
#include "storage/AppSettings.h"
#include "utils/I18n.h"

#ifdef WINDOWS
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/tcp.h>
#endif

constexpr int MAX_CLIENTS = 10;

TCPPairingServer::~TCPPairingServer() {
  Stop();
}

bool TCPPairingServer::Start(const PairingUIData &uiData) {
  if(m_IsRunning)
    return true;

  WSA_STARTUP
  PairingServer::Start(uiData);
  m_AcceptThread = std::thread(&TCPPairingServer::AcceptThread, this);
  return true;
}

void TCPPairingServer::Stop() {
  m_IsRunning = false;
  SOCKET_CLOSE(m_ServerSocket);
  if(m_AcceptThread.joinable())
    m_AcceptThread.join();
}

void TCPPairingServer::AcceptThread() {
  struct sockaddr_in address{};
  socklen_t addrLen = sizeof(address);
  auto settings = AppSettings::Get();
  auto clientSockets = std::vector<SOCKET>();
  auto clientThreads = std::vector<std::thread>();
  spdlog::info("Starting TCP pairing server...");

  if((m_ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == SOCKET_INVALID) {
    spdlog::error("socket() failed. (Code={})", SOCKET_LAST_ERROR);
    ReportError(I18n::Get("error_pairing_server_init_unk"));
    m_IsRunning = false;
    return;
  }

  int opt = 1;
#ifdef LINUX
  if(setsockopt(m_ServerSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&opt), sizeof(opt))) {
    spdlog::error("setsockopt(SO_REUSEADDR) failed. (Code={})", SOCKET_LAST_ERROR);
    ReportError(I18n::Get("error_pairing_server_init_unk"));
    goto threadEnd;
  }
#endif
  if(setsockopt(m_ServerSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&opt), sizeof(opt))) {
    spdlog::error("setsockopt(TCP_NODELAY) failed. (Code={})", SOCKET_LAST_ERROR);
    ReportError(I18n::Get("error_pairing_server_init_unk"));
    goto threadEnd;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(settings.pairingServerPort);
  if(bind(m_ServerSocket, reinterpret_cast<struct sockaddr *>(&address), sizeof(address)) < 0) {
    spdlog::error("bind() failed. (Code={})", SOCKET_LAST_ERROR);
    ReportError(I18n::Get("error_pairing_server_init"));
    goto threadEnd;
  }
  if(listen(m_ServerSocket, MAX_CLIENTS) < 0) {
    spdlog::error("listen() failed. (Code={})", SOCKET_LAST_ERROR);
    ReportError(I18n::Get("error_pairing_server_init_unk"));
    goto threadEnd;
  }
  if(!SetSocketBlocking(m_ServerSocket, false)) {
    spdlog::error("Failed setting server socket to non-blocking mode. (Code={})", SOCKET_LAST_ERROR);
    ReportError(I18n::Get("error_pairing_server_init_unk"));
    goto threadEnd;
  }

  spdlog::info("TCP pairing server started on port '{}'.", settings.pairingServerPort);
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
      ReportError(I18n::Get("error_pairing_server_init_unk"));
      break;
    }
    clientSockets.emplace_back(clientSocket);
    clientThreads.emplace_back(&TCPPairingServer::ClientThread, this, clientSocket);
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
  spdlog::info("TCP pairing server stopped.");
}

void TCPPairingServer::ClientThread(SOCKET clientSocket) {
  spdlog::info("TCP pairing client connected.");
  ++m_NumConnections;
  int opt = 1;
  if(setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&opt), sizeof(opt))) {
    spdlog::error("setsockopt(TCP_NODELAY) failed. (Code={})", SOCKET_LAST_ERROR);
  }
  SocketStream stream(clientSocket);
  HandleClient(stream);
  --m_NumConnections;
  SOCKET_CLOSE(clientSocket);
  spdlog::info("TCP pairing client closed.");
}
