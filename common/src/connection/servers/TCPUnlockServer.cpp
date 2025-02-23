#include "TCPUnlockServer.h"

#include "../SocketDefs.h"
#include "storage/AppSettings.h"
#include "storage/PairedDevicesStorage.h"

#ifdef WINDOWS
#include <Ws2tcpip.h>
#endif

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

void TCPUnlockServer::PerformAuthFlow(SOCKET socket) {
  auto devicePacket = ReadPacket(socket);
  if(devicePacket.error != PacketError::NONE) {
    spdlog::error("Packet error.");
    return;
  }
  auto pairingId = std::string((const char *)devicePacket.data.data(), devicePacket.data.size());
  auto device = PairedDevicesStorage::GetDeviceByID(pairingId);
  if(!device.has_value()) {
    spdlog::error("Invalid pairing ID.");
    return;
  }
  m_PairedDevice = device.value();
  BaseUnlockConnection::PerformAuthFlow(socket);
}

void TCPUnlockServer::AcceptThread() {
  struct sockaddr_in address{};
  socklen_t addrLen = sizeof(address);
  auto settings = AppSettings::Get();
  spdlog::info("Starting TCP server...");

  if((m_ServerSocket = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_INVALID) {
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

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(settings.unlockServerPort);
  if(bind(m_ServerSocket, (struct sockaddr *)&address, sizeof(address)) < 0) {
    spdlog::error("bind() failed. (Code={})", SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::PORT_ERROR;
    goto threadEnd;
  }
  if(listen(m_ServerSocket, 3) < 0) {
    spdlog::error("listen() failed. (Code={})", SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::UNK_ERROR;
    goto threadEnd;
  }

  spdlog::info("TCP server started on port '{}'.", settings.unlockServerPort);
  while(m_IsRunning) {
    SOCKET clientSocket;
    if((clientSocket = accept(m_ServerSocket, (struct sockaddr *)&address, (socklen_t *)&addrLen)) == SOCKET_INVALID) {
      auto err = SOCKET_LAST_ERROR;
      if(err != SOCKET_ERROR_CONNECT_ABORTED)
        spdlog::error("accept() failed. (Code={})", err);
      break;
    }
    if(!SetSocketBlocking(clientSocket, false)) {
      spdlog::error("Failed setting client socket to non-blocking mode. (Code={})", SOCKET_LAST_ERROR);
      m_UnlockState = UnlockState::UNK_ERROR;
      break;
    }
    m_ClientThreads.emplace_back(&TCPUnlockServer::ClientThread, this, clientSocket);
  }

threadEnd:
  m_IsRunning = false;
  m_HasConnection = false;
  SOCKET_CLOSE(m_ServerSocket);
  for(auto &thread : m_ClientThreads)
    if(thread.joinable())
      thread.join();
  m_ClientThreads.clear();
  spdlog::info("TCP server stopped.");
}

void TCPUnlockServer::ClientThread(SOCKET clientSocket) {
  spdlog::info("TCP client connected.");
  m_HasConnection = true;
  PerformAuthFlow(clientSocket);

  m_HasConnection = false;
  SOCKET_CLOSE(clientSocket);
  spdlog::info("TCP client closed.");
}
