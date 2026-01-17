#include "BaseUnlockConnection.h"

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

PacketUnlockResponse BaseUnlockConnection::GetResponseData() {
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

std::optional<PacketUnlockRequest> BaseUnlockConnection::GetUnlockInfoPacket() {
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

  // Decrypt data
  auto cryptResult = CryptUtils::DecryptAESPacket(packet.data, m_PairedDevice.encryptionKey);
  if(cryptResult.result != PacketCryptResult::OK) {
    auto respStr = std::string((const char *)packet.data.data(), packet.data.size());
    if(respStr == "CANCEL") {
      m_UnlockState = UnlockState::CANCELED;
      return;
    } else if(respStr == "NOT_PAIRED") {
      m_UnlockState = UnlockState::NOT_PAIRED_ERROR;
      return;
    } else if(respStr == "ERROR") {
      m_UnlockState = UnlockState::APP_ERROR;
      return;
    } else if(respStr == "TIME_ERROR") {
      m_UnlockState = UnlockState::TIME_ERROR;
      return;
    }

    switch(cryptResult.result) {
      case INVALID_TIMESTAMP: {
        spdlog::error("Invalid timestamp on AES data.");
        m_UnlockState = UnlockState::TIME_ERROR;
        break;
      }
      default: {
        spdlog::error("Invalid data received. (Size={}, Str={})", packet.data.size(), respStr);
        m_UnlockState = UnlockState::DATA_ERROR;
        break;
      }
    }
    return;
  }

  // Parse data
  auto dataStr = std::string(cryptResult.data.begin(), cryptResult.data.end());
  auto responsePacket = PacketUnlockResponse::FromJson(dataStr);
  if(responsePacket.has_value()) {
    m_ResponseData = responsePacket.value();
    if(m_ResponseData.unlockToken == m_UnlockToken) {
      m_UnlockState = UnlockState::SUCCESS;
    } else {
      m_UnlockState = UnlockState::UNK_ERROR;
    }
  } else {
    spdlog::error("Error parsing response data.");
    m_UnlockState = UnlockState::DATA_ERROR;
  }
}
