#include "PairingServer.h"

#include "platform/NetworkHelper.h"
#include "platform/PlatformHelper.h"
#include "storage/AppSettings.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/AppInfo.h"
#include "utils/CryptUtils.h"
#include "utils/I18n.h"
#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <Ws2tcpip.h>
#else
#include <netinet/tcp.h>
#endif

constexpr int MAX_CLIENTS = 10;

PairingServer::PairingServer(const std::function<void(const std::string&)>& errorCallback) {
  m_ErrorCallback = errorCallback;
}

PairingServer::~PairingServer() {
  BaseConnection::~BaseConnection();
  Stop();
}

bool PairingServer::Start(const PairingUIData &uiData) {
  m_UIData = uiData;
  if(m_IsRunning)
    return true;

  WSA_STARTUP
  m_IsRunning = true;
  m_AcceptThread = std::thread(&PairingServer::AcceptThread, this);
  return true;
}

void PairingServer::Stop() {
  SOCKET_CLOSE(m_ServerSocket);
  if(m_AcceptThread.joinable())
    m_AcceptThread.join();
  m_IsRunning = false;
}

void PairingServer::AcceptThread() {
  struct sockaddr_in address {};
  socklen_t addrLen = sizeof(address);
  auto settings = AppSettings::Get();
  auto clientSockets = std::vector<SOCKET>();
  auto clientThreads = std::vector<std::thread>();
  spdlog::info("Starting TCP pairing server...");

  if((m_ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == SOCKET_INVALID) {
    spdlog::error("socket() failed. (Code={})", SOCKET_LAST_ERROR);
    m_ErrorCallback(I18n::Get("error_pairing_server_init_unk"));
    m_IsRunning = false;
    return;
  }

  int opt = 1;
  if(setsockopt(m_ServerSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&opt), sizeof(opt))) {
    spdlog::error("setsockopt(TCP_NODELAY) failed. (Code={})", SOCKET_LAST_ERROR);
    m_ErrorCallback(I18n::Get("error_pairing_server_init_unk"));
    goto threadEnd;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(settings.pairingServerPort);
  if(bind(m_ServerSocket, reinterpret_cast<struct sockaddr *>(&address), sizeof(address)) < 0) {
    spdlog::error("bind() failed. (Code={})", SOCKET_LAST_ERROR);
    m_ErrorCallback(I18n::Get("error_pairing_server_init"));
    goto threadEnd;
  }
  if(listen(m_ServerSocket, MAX_CLIENTS) < 0) {
    spdlog::error("listen() failed. (Code={})", SOCKET_LAST_ERROR);
    m_ErrorCallback(I18n::Get("error_pairing_server_init_unk"));
    goto threadEnd;
  }
  if(!SetSocketBlocking(m_ServerSocket, false)) {
    spdlog::error("Failed setting server socket to non-blocking mode. (Code={})", SOCKET_LAST_ERROR);
    m_ErrorCallback(I18n::Get("error_pairing_server_init_unk"));
    goto threadEnd;
  }

  spdlog::info("TCP pairing server started on port '{}'.", settings.pairingServerPort);
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
      m_ErrorCallback(I18n::Get("error_pairing_server_init_unk"));
      break;
    }
    clientSockets.emplace_back(clientSocket);
    clientThreads.emplace_back(&PairingServer::ClientThread, this, clientSocket);
  }

threadEnd:
  for(auto clientSocket : clientSockets) {
    SOCKET_CLOSE(clientSocket);
  }
  SOCKET_CLOSE(m_ServerSocket);
  for(auto &thread : clientThreads)
    if(thread.joinable())
      thread.join();
  m_IsRunning = false;
  spdlog::info("TCP pairing server stopped.");
}

void PairingServer::ClientThread(SOCKET clientSocket) {
  spdlog::info("TCP pairing client connected.");
  int opt = 1;
  if(setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&opt), sizeof(opt))) {
    spdlog::error("setsockopt(TCP_NODELAY) failed. (Code={})", SOCKET_LAST_ERROR);
  }
  auto packetData = ReadEncryptedPacket(clientSocket);
  if(packetData.empty()) {
    spdlog::info("TCP pairing client closed.");
    SOCKET_CLOSE(clientSocket);
    return;
  }
  try {
    auto initPacket = PacketPairInit::FromJson({packetData.begin(), packetData.end()});
    if(!initPacket.has_value())
      throw std::runtime_error(I18n::Get("error_pairing_packet_parse"));
    if(AppInfo::CompareVersion(AppInfo::GetProtocolVersion(), initPacket->protoVersion) != 0)
      throw std::runtime_error(I18n::Get("error_protocol_mismatch"));
    if(initPacket.value().deviceUUID.empty()) {
      spdlog::warn("Device ID is empty. Generating fallback...");
      initPacket.value().deviceUUID = StringUtils::RandomString(32);
    }

    auto passwordKey = StringUtils::RandomString(64);
    auto pwEnc = CryptUtils::EncryptAES(m_UIData.password, passwordKey);
    if(!pwEnc.has_value())
      throw std::runtime_error(I18n::Get("error_password_encrypt"));

    auto device = PairedDevice();
    device.id = CryptUtils::Sha256(AppSettings::Get().machineID + initPacket->deviceUUID + m_UIData.userName);
    device.pairingMethod = m_UIData.method;
    device.deviceName = initPacket->deviceName;
    device.userName = m_UIData.userName;
    device.passwordEnc = pwEnc.value();
    device.encryptionKey = m_UIData.encKey;

    device.ipAddress = initPacket->ipAddress;
    device.tcpPort = initPacket->tcpPort;
    device.udpPort = initPacket->udpPort;
    device.udpManualPort = initPacket->udpManualPort;
    device.bluetoothAddress = m_UIData.btAddress;
    device.cloudToken = initPacket->cloudToken;

    auto respPacket = PacketPairResponse();
    respPacket.data = PacketPairResponseData();
    respPacket.data.deviceId = device.id;
    respPacket.data.deviceName = NetworkHelper::GetHostName();
    respPacket.data.deviceOS = AppInfo::GetOperatingSystem();
    respPacket.data.ipAddress = NetworkHelper::GetSavedNetworkInterface().ipAddress;
    respPacket.data.port = AppSettings::Get().unlockServerPort;
    respPacket.data.pairingMethod = device.pairingMethod;
    respPacket.data.macAddress = m_UIData.macAddress;
    respPacket.data.userName = m_UIData.userName;
    respPacket.data.passwordKey = passwordKey;

    WriteEncryptedPacket(clientSocket, PACKET_ID_PAIR_RESPONSE, respPacket.ToJson().dump());
    PairedDevicesStorage::AddDevice(device);
    spdlog::info("Successfully paired device. (ID={}, Method={})", device.id, PairingMethodUtils::ToString(device.pairingMethod));
#ifdef WINDOWS
    if(PlatformHelper::SetDefaultCredProv(m_UIData.userName, "{74A23DE2-B81D-46EC-E129-CD32507ED716}"))
      spdlog::info("Successfully changed default credential provider for user '{}'.", m_UIData.userName);
    else
      spdlog::error("Failed setting default credential provider for user '{}'.", m_UIData.userName);
#endif
  } catch(const std::exception &ex) {
    spdlog::error("Pairing server exception: {}", ex.what());
    auto respPacket = PacketPairResponse();
    respPacket.errMsg = ex.what();
    WriteEncryptedPacket(clientSocket, PACKET_ID_PAIR_RESPONSE, respPacket.ToJson().dump());
  }
  SOCKET_CLOSE(clientSocket);
  spdlog::info("TCP pairing client closed.");
}

std::vector<uint8_t> PairingServer::ReadEncryptedPacket(SOCKET clientSocket) const {
  auto packet = ReadPacket(clientSocket);
  if(packet.error != PacketError::NONE) {
    spdlog::error("Error reading pairing packet. (Code={})", static_cast<int>(packet.error));
    return {};
  }
  auto decRes = CryptUtils::DecryptAESPacket(packet.data, m_UIData.encKey);
  if(decRes.result != PacketCryptResult::OK) {
    spdlog::error("Error decrypting pairing packet. (Code={})", static_cast<int>(decRes.result));
    return {};
  }
  return decRes.data;
}

void PairingServer::WriteEncryptedPacket(SOCKET clientSocket, uint8_t packetId, const std::string &data) const {
  auto encRes = CryptUtils::EncryptAESPacket({data.begin(), data.end()}, m_UIData.encKey);
  if(encRes.result != PacketCryptResult::OK) {
    spdlog::error("Error encrypting pairing packet. (Size={}, Code={})", data.size(), static_cast<int>(encRes.result));
    return;
  }
  auto writeRes = WritePacket(clientSocket, packetId, {encRes.data.begin(), encRes.data.end()});
  if(writeRes != PacketError::NONE) {
    spdlog::error("Error writing pairing packet. (Code={})", static_cast<int>(writeRes));
  }
}
