#include "TCPUnlockClient.h"

#include "connection/SocketDefs.h"
#include "storage/AppSettings.h"

#include <algorithm>
#include <vector>

#ifdef WINDOWS
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/tcp.h>
#endif

TCPUnlockClient::TCPUnlockClient(const PairedDevice &device) : BaseUnlockConnection(device) {
  m_Port = device.tcpPort;
  m_ClientSocket = (SOCKET)SOCKET_INVALID;
  m_IsRunning = false;
}

bool TCPUnlockClient::Start() {
  if(m_IsRunning)
    return true;

  WSA_STARTUP
  m_IsRunning = true;
  m_AcceptThread = std::thread(&TCPUnlockClient::ConnectThread, this);
  return true;
}

void TCPUnlockClient::Stop() {
  if(!m_IsRunning)
    return;

  if(m_ClientSocket != SOCKET_INVALID && m_HasConnection)
    write(m_ClientSocket, "CLOSE", 5);

  m_IsRunning = false;
  m_HasConnection = false;
  SOCKET_CLOSE(m_ClientSocket);
  if(m_AcceptThread.joinable())
    m_AcceptThread.join();
}

void TCPUnlockClient::ConnectThread() {
  auto settings = AppSettings::Get();
  spdlog::info("Connecting via TCP...");

  std::vector<std::string> candidates{};
  auto addCandidate = [&candidates](const std::string &ipAddress) {
    if(!ipAddress.empty() && std::ranges::find(candidates, ipAddress) == candidates.end())
      candidates.emplace_back(ipAddress);
  };

  addCandidate(m_PairedDevice.lastSuccessfulIpAddress);
  addCandidate(m_PairedDevice.ipAddress);
  addCandidate(m_PairedDevice.secondaryIpAddress);

  if(candidates.empty()) {
    spdlog::error("No TCP IP addresses configured.");
    m_IsRunning = false;
    m_UnlockState = UnlockState::CONNECT_ERROR;
    return;
  }

  if(candidates.size() == 1) {
    for(uint32_t retry = 0; retry <= settings.clientConnectRetries && m_IsRunning; ++retry) {
      if(ConnectToAddress(candidates[0], settings.clientConnectTimeout))
        return;
    }
  } else {
    for(const auto &candidate : candidates) {
      if(!m_IsRunning)
        break;
      if(ConnectToAddress(candidate, 1))
        return;
    }
    for(const auto &candidate : candidates) {
      if(!m_IsRunning)
        break;
      if(ConnectToAddress(candidate, settings.clientConnectTimeout))
        return;
    }
  }

  m_IsRunning = false;
  m_HasConnection = false;
  m_UnlockState = UnlockState::CONNECT_ERROR;
}

bool TCPUnlockClient::ConnectToAddress(const std::string &ipAddress, uint32_t timeoutSeconds) {
  auto settings = AppSettings::Get();
  spdlog::info("Trying TCP address '{}' with timeout {}s...", ipAddress, timeoutSeconds);

  struct sockaddr_in serv_addr{};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons((u_short)m_Port);
  if(inet_pton(AF_INET, ipAddress.c_str(), &serv_addr.sin_addr) <= 0) {
    spdlog::error("Invalid IP address '{}'.", ipAddress);
    m_UnlockState = UnlockState::UNK_ERROR;
    return false;
  }

  if((m_ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == SOCKET_INVALID) {
    spdlog::error("socket() failed. (Code={})", SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::UNK_ERROR;
    return false;
  }

  fd_set fdSet{};
  FD_SET(m_ClientSocket, &fdSet);
  struct timeval connectTimeout{};
  connectTimeout.tv_sec = (long)timeoutSeconds;
  int opt = 1;
  int error = 0;
  socklen_t errorLen = sizeof(error);
  if(!SetSocketRWTimeout(m_ClientSocket, settings.clientSocketTimeout)) {
    spdlog::error("Failed setting R/W timeout for socket. (Code={})", SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::UNK_ERROR;
    goto threadEnd;
  }
  if(!SetSocketBlocking(m_ClientSocket, false)) {
    spdlog::error("Failed setting socket to non-blocking mode. (Code={})", SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::UNK_ERROR;
    goto threadEnd;
  }
  if(setsockopt(m_ClientSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&opt), sizeof(opt))) {
    spdlog::error("setsockopt(TCP_NODELAY) failed. (Code={})", SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::UNK_ERROR;
    goto threadEnd;
  }

  if(connect(m_ClientSocket, reinterpret_cast<struct sockaddr *>(&serv_addr), sizeof(serv_addr)) < 0) {
    auto error = SOCKET_LAST_ERROR;
    if(error != SOCKET_ERROR_IN_PROGRESS && error != SOCKET_ERROR_WOULD_BLOCK) {
      spdlog::error("connect() failed for '{}'. (Code={})", ipAddress, error);
      m_UnlockState = UnlockState::CONNECT_ERROR;
      goto threadEnd;
    }
  }
  if(select((int)m_ClientSocket + 1, nullptr, &fdSet, nullptr, &connectTimeout) <= 0) {
    spdlog::error("select() timed out or failed for '{}'. (Code={})", ipAddress, SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::CONNECT_ERROR;
    goto threadEnd;
  }

  if (getsockopt(m_ClientSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &errorLen) < 0) {
    spdlog::error("getsockopt(SO_ERROR) failed. (Code={})", SOCKET_LAST_ERROR);
    m_UnlockState = UnlockState::UNK_ERROR;
    goto threadEnd;
  }
  if (error != 0) {
    spdlog::error("getsockopt(SO_ERROR) returned an error for '{}'. (Code={})", ipAddress, error);
    m_UnlockState = UnlockState::CONNECT_ERROR;
    goto threadEnd;
  }

  m_HasConnection = true;
  m_PairedDevice.lastSuccessfulIpAddress = ipAddress;
  PerformAuthFlow(m_ClientSocket);
  m_IsRunning = false;
  m_HasConnection = false;
  SOCKET_CLOSE(m_ClientSocket);
  return true;

threadEnd:
  m_HasConnection = false;
  SOCKET_CLOSE(m_ClientSocket);
  return false;
}
