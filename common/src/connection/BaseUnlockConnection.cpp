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

UnlockResponseData BaseUnlockConnection::GetResponseData() {
  return m_ResponseData;
}

bool BaseUnlockConnection::HasClient() const {
  return m_HasConnection;
}

UnlockState BaseUnlockConnection::PollResult() {
  return m_UnlockState;
}

void BaseUnlockConnection::PerformAuthFlow(SOCKET socket) {
  auto serverDataStr = GetUnlockInfoPacket();
  if(serverDataStr.empty()) {
    m_UnlockState = UnlockState::UNK_ERROR;
    return;
  }
  auto writeResult = WritePacket(socket, {serverDataStr.begin(), serverDataStr.end()});
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

std::string BaseUnlockConnection::GetUnlockInfoPacket() {
  nlohmann::json encServerData = {{"authUser", m_AuthUser}, {"authProgram", m_AuthProgram}, {"unlockToken", m_UnlockToken}};
  auto encServerDataStr = encServerData.dump();
  auto cryptResult = CryptUtils::EncryptAESPacket({encServerDataStr.begin(), encServerDataStr.end()}, m_PairedDevice.encryptionKey);
  if(cryptResult.result != PacketCryptResult::OK) {
    spdlog::error("Encrypt error.");
    return {};
  }
  nlohmann::json serverData = {{"pairingId", m_PairedDevice.pairingId}, {"encData", StringUtils::ToHexString(cryptResult.data)}};
  return serverData.dump();
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
  try {
    auto json = nlohmann::json::parse(dataStr);
    auto response = UnlockResponseData();
    response.unlockToken = json["unlockToken"];
    response.password = json["password"];

    m_ResponseData = response;
    if(response.unlockToken == m_UnlockToken) {
      m_UnlockState = UnlockState::SUCCESS;
    } else {
      m_UnlockState = UnlockState::UNK_ERROR;
    }
  } catch(std::exception &) {
    spdlog::error("Error parsing response data.");
    m_UnlockState = UnlockState::DATA_ERROR;
  }
}
