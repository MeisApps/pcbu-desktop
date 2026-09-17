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

UnlockPhase BaseUnlockConnection::GetPhase() const {
  return m_IsRunning ? m_Phase.load() : UnlockPhase::FINISHED;
}

void BaseUnlockConnection::SetPhase(UnlockPhase phase) {
  m_Phase = phase;
}

UnlockState BaseUnlockConnection::PollResult() {
  return m_UnlockState;
}

void BaseUnlockConnection::PerformAuthFlow(ConnectionStream &stream, bool needsDeviceID) {
  SetPhase(UnlockPhase::PHONE_UNLOCKING);
  m_StateMutex.lock();
  m_ConnectionStates[&stream] = needsDeviceID ? UnlockConnectionState::NONE : UnlockConnectionState::HAS_DEVICE_ID;
  m_StateMutex.unlock();
  if(!needsDeviceID) {
    if(SendUnlockRequest(stream)) {
      m_StateMutex.lock();
      m_ConnectionStates[&stream] = UnlockConnectionState::HAS_UNLOCK_REQUEST;
      m_StateMutex.unlock();
    } else {
      return;
    }
  }

  auto packet = ReadPacket(stream);
  while(packet.error == PacketError::NONE) {
    OnPacketReceived(stream, packet);
    packet = ReadPacket(stream);
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
      case PacketError::UNSUPPORTED_VERSION: {
        m_UnlockState = UnlockState::PROTOCOL_ERROR;
        break;
      }
      case PacketError::PEER_UNAVAILABLE: {
        m_UnlockState = UnlockState::CLOUD_PHONE_UNREACHABLE;
        break;
      }
      default: {
        m_UnlockState = UnlockState::DATA_ERROR;
        break;
      }
    }
  }
}

void BaseUnlockConnection::OnPacketReceived(ConnectionStream &stream, Packet &packet) {
  std::unique_lock lock(m_StateMutex);
  auto connectionState = m_ConnectionStates[&stream];
  if(packet.id == PACKET_ID_DEVICE_ID && connectionState != UnlockConnectionState::NONE) {
    spdlog::error("Unexpected packet received. Got PACKET_ID_DEVICE_ID at state {}.", static_cast<int>(m_ConnectionStates[&stream]));
    return;
  }
  if(packet.id == PACKET_ID_UNLOCK_RESPONSE && connectionState != UnlockConnectionState::HAS_UNLOCK_REQUEST) {
    spdlog::error("Unexpected packet received. Got PACKET_ID_UNLOCK_RESPONSE at state {}.", static_cast<int>(m_ConnectionStates[&stream]));
    return;
  }

  switch(packet.id) {
    case PACKET_ID_DEVICE_ID: {
      auto deviceId = std::string(reinterpret_cast<const char *>(packet.data.data()), packet.data.size());
      auto device = PairedDevicesStorage::GetDeviceByID(deviceId);
      if(!device.has_value()) {
        spdlog::error("Invalid device ID.");
        m_UnlockState = UnlockState::DATA_ERROR;
        return;
      }
      m_PairedDevice = device.value();
      if(SendUnlockRequest(stream)) {
        m_ConnectionStates[&stream] = UnlockConnectionState::HAS_UNLOCK_REQUEST;
      }
      break;
    }
    case PACKET_ID_UNLOCK_RESPONSE: {
      OnResponseReceived(packet);
      break;
    }
    default: {
      spdlog::error("Invalid response packet. (ID={0:X}, State={1})", packet.id, static_cast<int>(m_ConnectionStates[&stream]));
      break;
    }
  }
}

bool BaseUnlockConnection::SendUnlockRequest(ConnectionStream &stream) {
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
  requestPacket.protoVersion = AppInfo::GetUnlockProtocolVersion();
  requestPacket.deviceId = m_PairedDevice.id;
  requestPacket.encData = StringUtils::ToHexString(cryptResult.data);
  auto requestStr = requestPacket.ToJson().dump();
  spdlog::debug("Writing PacketUnlockRequest...");
  auto writeResult = WritePacket(stream, PACKET_ID_UNLOCK_REQUEST, {requestStr.begin(), requestStr.end()});
  if(writeResult != PacketError::NONE) {
    switch(writeResult) {
      case PacketError::CLOSED_CONNECTION:
        m_UnlockState = UnlockState::CONNECT_ERROR;
        break;
      case PacketError::TIMEOUT:
        m_UnlockState = UnlockState::TIMEOUT;
        break;
      case PacketError::UNSUPPORTED_VERSION:
        m_UnlockState = UnlockState::PROTOCOL_ERROR;
        break;
      case PacketError::PEER_UNAVAILABLE:
        m_UnlockState = UnlockState::CLOUD_PHONE_UNREACHABLE;
        break;
      default:
        m_UnlockState = UnlockState::UNK_ERROR;
        break;
    }
    spdlog::error("Failed to write unlock request packet. (WriteResult={}, UnlockState={})", static_cast<int>(writeResult),
                  UnlockStateUtils::ToString(m_UnlockState));
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
