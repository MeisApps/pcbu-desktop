#include "PairingServer.h"

#include "platform/NetworkHelper.h"
#include "platform/PlatformHelper.h"
#include "storage/AppSettings.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/AppInfo.h"
#include "utils/CryptUtils.h"
#include "utils/I18n.h"
#include "utils/StringUtils.h"

PairingServer::PairingServer()
    : m_Acceptor(m_IOService, boostnet::tcp::endpoint(boostnet::tcp::v4(), AppSettings::Get().pairingServerPort)), m_Socket(m_IOService) {}

PairingServer::~PairingServer() {
  BaseConnection::~BaseConnection();
  Stop();
}

void PairingServer::Start(const PairingUIData &uiData) {
  m_UIData = uiData;
  if(m_IsRunning)
    return;
  m_IsRunning = true;
  m_AcceptThread = std::thread([this]() {
    spdlog::info("Pairing server started.");
    try {
      m_Acceptor.listen();
      Accept();
      m_IOService.run();
    } catch(const std::exception &ex) {
      spdlog::error("Pairing server error: {}", ex.what());
    }
    spdlog::info("Pairing server stopped.");
  });
}

void PairingServer::Stop() {
  if(!m_IsRunning)
    return;
  try {
    if(m_Socket.is_open()) {
      try {
        m_Socket.shutdown(boost::asio::socket_base::shutdown_both);
      } catch(...) {
      }
      m_Socket.close();
    }
    if(m_Acceptor.is_open())
      m_Acceptor.close();
    m_IOService.stop();
    if(m_AcceptThread.joinable())
      m_AcceptThread.join();
    m_IOService.restart();
    m_Acceptor = boostnet::tcp::acceptor(m_IOService, boostnet::tcp::endpoint(boostnet::tcp::v4(), AppSettings::Get().pairingServerPort));
    m_Socket = boostnet::tcp::socket(m_IOService);
  } catch(const std::exception &ex) {
    spdlog::error("Error stopping pairing server: {}", ex.what());
  }
  m_IsRunning = false;
}

void PairingServer::Accept() {
  m_Acceptor.async_accept(m_Socket, [this](const boost::system::error_code &error) {
    if(error) {
      spdlog::error("Error accepting socket: {}", error.message());
      return;
    }
    m_Socket.set_option(boostnet::tcp::no_delay(true));
    auto packetData = ReadEncryptedPacket();
    if(packetData.empty()) {
      try {
        m_Socket.shutdown(boost::asio::socket_base::shutdown_both);
      } catch(...) {
      }
      m_Socket.close();
      Accept();
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

      WriteEncryptedPacket(PACKET_ID_PAIR_RESPONSE, respPacket.ToJson().dump());
      PairedDevicesStorage::AddDevice(device);
      spdlog::info("Successfully paired device. (ID={}, Method={})", device.id, PairingMethodUtils::ToString(device.pairingMethod));

#ifdef WINDOWS
      if(PlatformHelper::SetDefaultCredProv(m_UIData.userName, "{74A23DE2-B81D-46EC-E129-CD32507ED716}"))
        spdlog::info("Successfully changed default credential provider for user '{}'.", m_UIData.userName);
      else
        spdlog::error("Failed setting default credential provider for user '{}'.", m_UIData.userName);
#endif
    } catch(const std::exception &ex) {
      spdlog::error("Pairing server error: {}", ex.what());
      auto respPacket = PacketPairResponse();
      respPacket.errMsg = ex.what();
      WriteEncryptedPacket(PACKET_ID_PAIR_RESPONSE, respPacket.ToJson().dump());
    }
    try {
      m_Socket.shutdown(boost::asio::socket_base::shutdown_both);
    } catch(...) {
    }
    m_Socket.close();
    Accept();
  });
}

std::vector<uint8_t> PairingServer::ReadEncryptedPacket() {
  auto packet = ReadPacket(m_Socket.native_handle());
  if(packet.error != PacketError::NONE) {
    spdlog::warn("Error reading packet. (Code={})", (int)packet.error);
    return {};
  }
  auto decRes = CryptUtils::DecryptAESPacket(packet.data, m_UIData.encKey);
  if(decRes.result != PacketCryptResult::OK) {
    spdlog::warn("Error decrypting packet. (Code={})", (int)decRes.result);
    return {};
  }
  return decRes.data;
}

void PairingServer::WriteEncryptedPacket(uint8_t packetId, const std::string &data) {
  auto encRes = CryptUtils::EncryptAESPacket({data.begin(), data.end()}, m_UIData.encKey);
  if(encRes.result != PacketCryptResult::OK) {
    spdlog::warn("Error encrypting packet. (Size={}, Code={})", data.size(), (int)encRes.result);
    return;
  }
  WritePacket(m_Socket.native_handle(), packetId, {encRes.data.begin(), encRes.data.end()});
}
