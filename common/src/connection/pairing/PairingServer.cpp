#include "PairingServer.h"

#include "platform/NetworkHelper.h"
#include "platform/PlatformHelper.h"
#include "storage/AppSettings.h"
#include "storage/PairedDevicesStorage.h"
#include "storage/PairingMethod.h"
#include "utils/AppInfo.h"
#include "utils/CryptUtils.h"
#include "utils/I18n.h"
#include "utils/StringUtils.h"

PairingServer::PairingServer(const std::function<void(const std::string &)> &errorCallback) {
  m_ErrorCallback = errorCallback;
}

bool PairingServer::Start(const PairingUIData &uiData) {
  m_UIData = uiData;
  m_IsRunning = true;
  return true;
}

void PairingServer::ReportError(const std::string &message) {
  if(!m_IsRunning)
    return;
  m_ErrorCallback(message);
}

void PairingServer::HandleClient(ConnectionStream &stream) {
  auto isCloud = m_UIData.method == PairingMethod::CLOUD;
  auto packetRes = ReadEncryptedPacket(stream);
  if(packetRes.data.empty()) {
    if(packetRes.result == PacketCryptResult::INVALID_TIMESTAMP) {
      ReportError(I18n::Get("error_aes_time_mismatch"));
      return;
    }
    if(packetRes.result == PacketCryptResult::PACKET_ERROR) {
      if(packetRes.error == PacketError::UNSUPPORTED_VERSION) {
        ReportError(I18n::Get("error_protocol_mismatch"));
        return;
      }
      if(packetRes.error == PacketError::TIMEOUT || packetRes.error == PacketError::PEER_UNAVAILABLE) {
        ReportError(I18n::Get("error_pairing_timeout"));
        return;
      }
    }
    if(isCloud)
      ReportError(I18n::Get("error_cloud_pairing"));
    return;
  }
  try {
    auto initPacket = PacketPairInit::FromJson({packetRes.data.begin(), packetRes.data.end()});
    if(!initPacket.has_value())
      throw std::runtime_error(I18n::Get("error_pairing_packet_parse"));
    if(AppInfo::CompareVersion(AppInfo::GetPairingProtocolVersion(), initPacket->protoVersion) != 0)
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
    respPacket.data.unlockServerPort = AppSettings::Get().unlockServerPort;
    respPacket.data.pairingMethod = device.pairingMethod;
    for(const auto &netIf : NetworkHelper::GetWakeOnLanInterfaces())
      respPacket.data.macAddresses.emplace_back(netIf.macAddress);
    respPacket.data.userName = m_UIData.userName;
    respPacket.data.passwordKey = passwordKey;

    if(WriteEncryptedPacket(stream, PACKET_ID_PAIR_RESPONSE, respPacket.ToJson().dump())) {
      PairedDevicesStorage::AddDevice(device);
      spdlog::info("Successfully paired device. (ID={}, Method={})", device.id, PairingMethodUtils::ToString(device.pairingMethod));
#ifdef WINDOWS
      if(PlatformHelper::SetDefaultCredProv(m_UIData.userName, "{74A23DE2-B81D-46EC-E129-CD32507ED716}"))
        spdlog::info("Successfully changed default credential provider for user '{}'.", m_UIData.userName);
      else
        spdlog::error("Failed setting default credential provider for user '{}'.", m_UIData.userName);
#endif
    } else {
      ReportError(I18n::Get("error_pairing_failed"));
    }
  } catch(const std::exception &ex) {
    spdlog::error("Pairing server exception: {}", ex.what());
    auto respPacket = PacketPairResponse();
    respPacket.errMsg = ex.what();
    if(!WriteEncryptedPacket(stream, PACKET_ID_PAIR_RESPONSE, respPacket.ToJson().dump())) {
      ReportError(ex.what());
    }
  }
}

CryptPacket PairingServer::ReadEncryptedPacket(ConnectionStream &stream) const {
  auto packet = ReadPacket(stream);
  if(packet.error != PacketError::NONE) {
    spdlog::error("Error reading pairing packet. (Code={})", static_cast<int>(packet.error));
    CryptPacket result{};
    result.result = PacketCryptResult::PACKET_ERROR;
    result.error = packet.error;
    return result;
  }
  auto decRes = CryptUtils::DecryptAESPacket(packet.data, m_UIData.encKey);
  if(decRes.result != PacketCryptResult::OK)
    spdlog::error("Error decrypting pairing packet. (Code={})", static_cast<int>(decRes.result));
  return decRes;
}

bool PairingServer::WriteEncryptedPacket(ConnectionStream &stream, uint16_t packetId, const std::string &data) const {
  auto encRes = CryptUtils::EncryptAESPacket({data.begin(), data.end()}, m_UIData.encKey);
  if(encRes.result != PacketCryptResult::OK) {
    spdlog::error("Error encrypting pairing packet. (Size={}, Code={})", data.size(), static_cast<int>(encRes.result));
    return false;
  }
  auto writeRes = WritePacket(stream, packetId, {encRes.data.begin(), encRes.data.end()});
  if(writeRes != PacketError::NONE) {
    spdlog::error("Error writing pairing packet. (Code={})", static_cast<int>(writeRes));
    return false;
  }
  return true;
}
