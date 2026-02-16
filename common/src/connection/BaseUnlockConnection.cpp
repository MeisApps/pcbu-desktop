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

void BaseUnlockConnection::PerformAuthFlow(SOCKET socket, bool needsDeviceID) {
  m_StateMutex.lock();
  m_ConnectionStates[socket] = needsDeviceID ? UnlockConnectionState::NONE : UnlockConnectionState::HAS_DEVICE_ID;
  m_StateMutex.unlock();
  if(!needsDeviceID) {
    if(SendUnlockRequest(socket)) {
      m_StateMutex.lock();
      m_ConnectionStates[socket] = UnlockConnectionState::HAS_UNLOCK_REQUEST;
      m_StateMutex.unlock();
    } else {
      return;
    }
  }

  auto packet = ReadPacket(socket);
  while(packet.error == PacketError::NONE || packet.error == PacketError::UNKNOWN) {
    if(packet.error == PacketError::NONE)
      OnPacketReceived(socket, packet);
    packet = ReadPacket(socket);
  }
  if(m_UnlockState == UnlockState::UNKNOWN) {
    switch(packet.error) {
      case PacketError::CLOSED_CONNECTION: {
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
  }
}

void BaseUnlockConnection::OnPacketReceived(SOCKET socket, Packet &packet) {
  std::unique_lock lock(m_StateMutex);
  auto connectionState = m_ConnectionStates[socket];
  if(packet.id == PACKET_ID_DEVICE_ID && connectionState != UnlockConnectionState::NONE) {
    spdlog::error("Unexpected packet received. Got PACKET_ID_DEVICE_ID at state {}.", static_cast<int>(m_ConnectionStates[socket]));
    return;
  }
  if(packet.id == PACKET_ID_UNLOCK_RESPONSE && connectionState != UnlockConnectionState::HAS_UNLOCK_REQUEST) {
    spdlog::error("Unexpected packet received. Got PACKET_ID_UNLOCK_RESPONSE at state {}.", static_cast<int>(m_ConnectionStates[socket]));
    return;
  }

  switch(packet.id) {
    case PACKET_ID_DEVICE_ID: {
      auto deviceId = std::string(reinterpret_cast<const char *>(packet.data.data()), packet.data.size());
      auto device = PairedDevicesStorage::GetDeviceByID(deviceId);
      if(!device.has_value()) {
        spdlog::error("Invalid device ID.");
        return;
      }
      m_PairedDevice = device.value();
      if(SendUnlockRequest(socket)) {
        m_ConnectionStates[socket] = UnlockConnectionState::HAS_UNLOCK_REQUEST;
      }
      break;
    }
    case PACKET_ID_UNLOCK_RESPONSE: {
      OnResponseReceived(packet);
      break;
    }
    default: {
      spdlog::error("Invalid response packet. (ID={0:X}, State={1})", packet.id, static_cast<int>(m_ConnectionStates[socket]));
      break;
    }
  }
}

bool BaseUnlockConnection::SendUnlockRequest(SOCKET socket) {
  auto encData = PacketUnlockRequestData();
  encData.user = m_AuthUser;
  encData.program = m_AuthProgram;
  encData.unlockToken = m_UnlockToken;
  auto encDataStr = encData.ToJson().dump();
  auto cryptResult = CryptUtils::EncryptAESPacket({encDataStr.begin(), encDataStr.end()}, m_PairedDevice.encryptionKey);
  if(cryptResult.result != PacketCryptResult::OK) {
    spdlog::error("Failed to encrypt unlock request packet.");
    m_UnlockState = UnlockState::UNK_ERROR;
    return false;
  }
  auto requestPacket = PacketUnlockRequest();
  requestPacket.protoVersion = AppInfo::GetProtocolVersion();
  requestPacket.deviceId = m_PairedDevice.id;
  requestPacket.encData = StringUtils::ToHexString(cryptResult.data);
  auto requestStr = requestPacket.ToJson().dump();
  spdlog::debug("Writing PacketUnlockRequest...");
  auto writeResult = WritePacket(socket, PACKET_ID_UNLOCK_REQUEST, {requestStr.begin(), requestStr.end()});
  if(writeResult != PacketError::NONE) {
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
    spdlog::error("Failed to write unlock request packet. (WriteResult={}, UnlockState={})", static_cast<int>(writeResult), UnlockStateUtils::ToString(m_UnlockState));
    return false;
  }
  return true;
}

void BaseUnlockConnection::OnResponseReceived(const Packet &packet) {
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
