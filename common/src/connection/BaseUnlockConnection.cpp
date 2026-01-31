#include "BaseUnlockConnection.h"

#include "utils/AppInfo.h"
#include "utils/StringUtils.h"

BaseUnlockConnection::BaseUnlockConnection() {
  m_UnlockToken = StringUtils::RandomString(64);
  m_UnlockState = UnlockState::UNKNOWN;
}

BaseUnlockConnection::BaseUnlockConnection(const PairedDevice &device) : BaseUnlockConnection() {
  m_PairedDevice = device;
}

BaseUnlockConnection::~BaseUnlockConnection() {
  if(m_AcceptThread.joinable())
    m_AcceptThread.join();
}

void BaseUnlockConnection::SetUnlockInfo(const std::string &authUser, const std::string &authProgram) {
  m_AuthUser = authUser;
  m_AuthProgram = authProgram;
}

PairedDevice BaseUnlockConnection::GetDevice() {
  return m_PairedDevice;
}

PacketUnlockResponseData BaseUnlockConnection::GetResponseData() {
  return m_ResponseData;
}

bool BaseUnlockConnection::HasClient() const {
  return m_HasConnection;
}

UnlockState BaseUnlockConnection::PollResult() {
  return m_UnlockState;
}

void BaseUnlockConnection::PerformAuthFlow(SOCKET socket) {
  auto requestPacket = GetUnlockInfoPacket();
  if(!requestPacket.has_value()) {
    m_UnlockState = UnlockState::UNK_ERROR;
    return;
  }
  auto requestStr = requestPacket.value().ToJson().dump();
  auto writeResult = WritePacket(socket, {requestStr.begin(), requestStr.end()});
  if(writeResult == PacketError::NONE) {
    auto responsePacket = ReadPacket(socket);
    OnResponseReceived(responsePacket);
  } else {
    switch(writeResult) {
      case PacketError::CLOSED_CONNECTION:
        m_UnlockState = UnlockState::CONNECT_ERROR;
        break;
      case PacketError::TIMEOUT:
        m_UnlockState = UnlockState::TIMEOUT;
        break;
      default:
        m_UnlockState = UnlockState::UNK_ERROR;
        break;
    }
  }
}

std::optional<PacketUnlockRequest> BaseUnlockConnection::GetUnlockInfoPacket() const {
  auto encData = PacketUnlockRequestData();
  encData.user = m_AuthUser;
  encData.program = m_AuthProgram;
  encData.unlockToken = m_UnlockToken;
  auto encDataStr = encData.ToJson().dump();
  auto cryptResult = CryptUtils::EncryptAESPacket({encDataStr.begin(), encDataStr.end()}, m_PairedDevice.encryptionKey);
  if(cryptResult.result != PacketCryptResult::OK) {
    spdlog::error("Encrypt error.");
    return {};
  }
  auto request = PacketUnlockRequest();
  request.protoVersion = AppInfo::GetProtocolVersion();
  request.deviceId = m_PairedDevice.id;
  request.encData = StringUtils::ToHexString(cryptResult.data);
  return request;
}

void BaseUnlockConnection::OnResponseReceived(const Packet &packet) {
  // Check packet
  if(packet.error != PacketError::NONE) {
    switch(packet.error) {
      case PacketError::CLOSED_CONNECTION: {
        spdlog::error("Connection closed.");
        m_UnlockState = UnlockState::CONNECT_ERROR;
        break;
      }
      case PacketError::TIMEOUT: {
        m_UnlockState = UnlockState::TIMEOUT;
        break;
      }
      default: {
        m_UnlockState = UnlockState::DATA_ERROR;
        break;
      }
    }
    return;
  }

  // Parse data
  auto respStr = std::string(packet.data.begin(), packet.data.end());
  auto responsePacket = PacketUnlockResponse::FromJson(respStr);
  if(!responsePacket.has_value()) {
    spdlog::error("Error parsing response packet.");
    m_UnlockState = UnlockState::DATA_ERROR;
    return;
  }

  // Error handling
  auto error = responsePacket.value().error;
  if(!error.empty()) {
    spdlog::error("Error in response packet: {}", error);
    if(error == "CANCEL") {
      m_UnlockState = UnlockState::CANCELED;
    } else if(error == "NOT_PAIRED") {
      m_UnlockState = UnlockState::NOT_PAIRED_ERROR;
    } else if(error == "APP_ERROR") {
      m_UnlockState = UnlockState::APP_ERROR;
    } else if(error == "TIME_ERROR") {
      m_UnlockState = UnlockState::TIME_ERROR;
    } else if(error == "DATA_ERROR") {
      m_UnlockState = UnlockState::DATA_ERROR;
    } else if(error == "PROTOCOL_ERROR") {
      m_UnlockState = UnlockState::PROTOCOL_ERROR;
    } else {
      m_UnlockState = UnlockState::UNK_ERROR;
    }
    return;
  }

  // Decrypt data
  auto cryptData = StringUtils::FromHexString(responsePacket.value().encData);
  auto cryptResult = CryptUtils::DecryptAESPacket(cryptData, m_PairedDevice.encryptionKey);
  if(cryptResult.result != PacketCryptResult::OK) {
    switch(cryptResult.result) {
      case INVALID_TIMESTAMP: {
        spdlog::error("Invalid timestamp on AES data.");
        m_UnlockState = UnlockState::TIME_ERROR;
        break;
      }
      default: {
        spdlog::error("Invalid data received. (Size={})", packet.data.size());
        m_UnlockState = UnlockState::DATA_ERROR;
        break;
      }
    }
    return;
  }

  // Parse encrypted data
  auto dataStr = std::string(cryptResult.data.begin(), cryptResult.data.end());
  auto dataPacket = PacketUnlockResponseData::FromJson(dataStr);
  if(!dataPacket.has_value()) {
    spdlog::error("Error parsing response data.");
    m_UnlockState = UnlockState::DATA_ERROR;
    return;
  }
  m_ResponseData = dataPacket.value();

  // Token check
  if(m_ResponseData.unlockToken == m_UnlockToken) {
    m_UnlockState = UnlockState::SUCCESS;
  } else {
    m_UnlockState = UnlockState::UNK_ERROR;
  }
}
