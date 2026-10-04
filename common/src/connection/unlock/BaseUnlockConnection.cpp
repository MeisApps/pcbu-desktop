#include "BaseUnlockConnection.h"

#include "utils/AppInfo.h"
#include "utils/StringUtils.h"

BaseUnlockConnection::BaseUnlockConnection() {
  m_UnlockToken = StringUtils::RandomString(64);
  m_UnlockState = UnlockState::UNKNOWN;
}

BaseUnlockConnection::BaseUnlockConnection(const PairedDevice &device) : BaseUnlockConnection() {
  m_PairedDevice = device;
  m_AllowedDevices = {device};
}

BaseUnlockConnection::~BaseUnlockConnection() {
  if(m_AcceptThread.joinable())
    m_AcceptThread.join();
}

void BaseUnlockConnection::SetUnlockInfo(const std::string &authUser, const std::string &authProgram) {
  m_AuthUser = authUser;
  m_AuthProgram = authProgram;
}

void BaseUnlockConnection::SetAllowedDevices(const std::vector<PairedDevice> &devices) {
  std::lock_guard lock(m_StateMutex);
  m_AllowedDevices = devices;
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
  PairedDevice device{};
  {
    std::lock_guard lock(m_StateMutex);
    auto &info = m_Connections[&stream];
    info.state = needsDeviceID ? UnlockConnectionState::NONE : UnlockConnectionState::HAS_DEVICE_ID;
    info.device = needsDeviceID ? PairedDevice() : m_PairedDevice;
    device = info.device;
  }

  auto packet = Packet{PacketError::NONE};
  auto isOpen = true;
  if(!needsDeviceID) {
    auto writeError = SendUnlockRequest(stream, device);
    if(writeError == PacketError::NONE) {
      std::lock_guard lock(m_StateMutex);
      m_Connections[&stream].state = UnlockConnectionState::HAS_UNLOCK_REQUEST;
    } else {
      m_UnlockState = MapPacketError(writeError, UnlockState::UNK_ERROR);
      isOpen = false;
    }
  }

  if(isOpen) {
    packet = ReadPacket(stream);
    while(packet.error == PacketError::NONE) {
      if(!OnPacketReceived(stream, packet, needsDeviceID))
        break;
      packet = ReadPacket(stream);
    }
  }
  {
    std::lock_guard lock(m_StateMutex);
    m_Connections.erase(&stream);
  }

  if(packet.error == PacketError::NONE || m_UnlockState != UnlockState::UNKNOWN)
    return;
  if(needsDeviceID) {
    spdlog::warn("Server connection ended. (Error={})", static_cast<int>(packet.error));
    return;
  }
  m_UnlockState = MapPacketError(packet.error, UnlockState::DATA_ERROR);
}

bool BaseUnlockConnection::OnPacketReceived(ConnectionStream &stream, const Packet &packet, bool isServerConnection) {
  ConnectionInfo info{};
  {
    std::lock_guard lock(m_StateMutex);
    info = m_Connections[&stream];
  }
  if(packet.id == PACKET_ID_DEVICE_ID && info.state != UnlockConnectionState::NONE) {
    spdlog::error("Unexpected packet received. Got PACKET_ID_DEVICE_ID at state {}.", static_cast<int>(info.state));
    return true;
  }
  if(packet.id == PACKET_ID_UNLOCK_RESPONSE && info.state != UnlockConnectionState::HAS_UNLOCK_REQUEST) {
    spdlog::error("Unexpected packet received. Got PACKET_ID_UNLOCK_RESPONSE at state {}.", static_cast<int>(info.state));
    return true;
  }

  switch(packet.id) {
    case PACKET_ID_DEVICE_ID: {
      auto deviceId = std::string(reinterpret_cast<const char *>(packet.data.data()), packet.data.size());
      auto device = FindAllowedDevice(deviceId);
      if(!device.has_value()) {
        spdlog::error("Unknown or not allowed device ID.");
        return HandleUnverifiedError(UnlockState::DATA_ERROR, isServerConnection);
      }
      auto writeError = SendUnlockRequest(stream, device.value());
      if(writeError != PacketError::NONE)
        return HandleUnverifiedError(MapPacketError(writeError, UnlockState::UNK_ERROR), isServerConnection);
      std::lock_guard lock(m_StateMutex);
      auto &connection = m_Connections[&stream];
      connection.state = UnlockConnectionState::HAS_UNLOCK_REQUEST;
      connection.device = device.value();
      return true;
    }
    case PACKET_ID_UNLOCK_RESPONSE: {
      return OnResponseReceived(stream, packet, info.device, isServerConnection);
    }
    default: {
      spdlog::error("Invalid response packet. (ID={0:X}, State={1})", packet.id, static_cast<int>(info.state));
      return true;
    }
  }
}

PacketError BaseUnlockConnection::SendUnlockRequest(ConnectionStream &stream, const PairedDevice &device) {
  auto encData = PacketUnlockRequestData();
  encData.user = m_AuthUser;
  encData.program = m_AuthProgram;
  encData.unlockToken = m_UnlockToken;
  auto encDataStr = encData.ToJson().dump();
  auto cryptResult = CryptUtils::EncryptAESPacket({encDataStr.begin(), encDataStr.end()}, device.encryptionKey);
  if(cryptResult.result != PacketCryptResult::OK) {
    spdlog::error("Failed to encrypt unlock request packet.");
    return PacketError::UNKNOWN;
  }
  auto requestPacket = PacketUnlockRequest();
  requestPacket.protoVersion = AppInfo::GetUnlockProtocolVersion();
  requestPacket.deviceId = device.id;
  requestPacket.encData = StringUtils::ToHexString(cryptResult.data);
  auto requestStr = requestPacket.ToJson().dump();
  spdlog::debug("Writing PacketUnlockRequest...");
  auto writeResult = WritePacket(stream, PACKET_ID_UNLOCK_REQUEST, {requestStr.begin(), requestStr.end()});
  if(writeResult != PacketError::NONE)
    spdlog::error("Failed to write unlock request packet. (WriteResult={})", static_cast<int>(writeResult));
  return writeResult;
}

bool BaseUnlockConnection::OnResponseReceived(ConnectionStream &stream, const Packet &packet, const PairedDevice &device, bool isServerConnection) {
  // Parse data
  auto respStr = std::string(packet.data.begin(), packet.data.end());
  auto responsePacket = PacketUnlockResponse::FromJson(respStr);
  if(!responsePacket.has_value()) {
    spdlog::error("Error parsing response packet.");
    return HandleUnverifiedError(UnlockState::DATA_ERROR, isServerConnection);
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
    return true;
  }

  // Decrypt data
  auto cryptData = StringUtils::FromHexString(responsePacket.value().encData);
  auto cryptResult = CryptUtils::DecryptAESPacket(cryptData, device.encryptionKey);
  if(cryptResult.result != PacketCryptResult::OK) {
    if(cryptResult.result == INVALID_TIMESTAMP) {
      spdlog::error("Invalid timestamp on AES data.");
      m_UnlockState = UnlockState::TIME_ERROR;
      return true;
    }
    spdlog::error("Invalid data received. (Size={})", packet.data.size());
    return HandleUnverifiedError(UnlockState::DATA_ERROR, isServerConnection);
  }

  // Parse encrypted data
  auto dataStr = std::string(cryptResult.data.begin(), cryptResult.data.end());
  auto dataPacket = PacketUnlockResponseData::FromJson(dataStr);
  if(!dataPacket.has_value()) {
    spdlog::error("Error parsing response data.");
    m_UnlockState = UnlockState::DATA_ERROR;
    return true;
  }

  // Token check
  if(dataPacket.value().unlockToken != m_UnlockToken) {
    spdlog::error("Invalid unlock token.");
    return HandleUnverifiedError(UnlockState::UNK_ERROR, isServerConnection);
  }

  std::lock_guard lock(m_StateMutex);
  if(m_UnlockState != UnlockState::UNKNOWN)
    return true;
  m_PairedDevice = device;
  m_ResponseData = dataPacket.value();
  m_UnlockState = UnlockState::SUCCESS;
  {
    auto it = m_Connections.find(&stream);
    if(it != m_Connections.end())
      it->second.state = UnlockConnectionState::HAS_UNLOCK_RESPONSE;
  }
  return true;
}

bool BaseUnlockConnection::HandleUnverifiedError(UnlockState state, bool isServerConnection) {
  if(isServerConnection) {
    spdlog::warn("Closing server connection. (State={})", UnlockStateUtils::ToString(state));
    return false;
  }
  m_UnlockState = state;
  return true;
}

std::optional<PairedDevice> BaseUnlockConnection::FindAllowedDevice(const std::string &deviceId) {
  std::lock_guard lock(m_StateMutex);
  for(const auto &device : m_AllowedDevices)
    if(device.id == deviceId)
      return device;
  return {};
}

UnlockState BaseUnlockConnection::MapPacketError(PacketError error, UnlockState fallback) {
  switch(error) {
    case PacketError::CLOSED_CONNECTION:
      return UnlockState::CONNECT_ERROR;
    case PacketError::TIMEOUT:
      return UnlockState::TIMEOUT;
    case PacketError::UNSUPPORTED_VERSION:
      return UnlockState::PROTOCOL_ERROR;
    case PacketError::PEER_UNAVAILABLE:
      return UnlockState::CLOUD_PHONE_UNREACHABLE;
    default:
      return fallback;
  }
}
