#ifndef PCBU_DESKTOP_PACKETS_H
#define PCBU_DESKTOP_PACKETS_H

#include <cstdint>
#include <nlohmann/json.hpp>

#include "storage/PairingMethod.h"

constexpr uint32_t PACKET_HEADER = 0x4E8A5A5C;

constexpr uint8_t PACKET_ID_PAIR_INIT = 0x50;
constexpr uint8_t PACKET_ID_PAIR_RESPONSE = 0x51;

constexpr uint8_t PACKET_ID_DEVICE_ID = 0xB0;
constexpr uint8_t PACKET_ID_UNLOCK_REQUEST = 0xB1;
constexpr uint8_t PACKET_ID_UNLOCK_RESPONSE = 0xB2;

struct PacketPairInit { // From phone
  std::string protoVersion{};
  std::string deviceUUID{};
  std::string deviceName{};
  std::string ipAddress{};
  uint16_t tcpPort{};
  uint16_t udpPort{};
  uint16_t udpManualPort{};
  std::string cloudToken{};

  static std::optional<PacketPairInit> FromJson(const std::string &jsonStr) {
    try {
      auto json = nlohmann::json::parse(jsonStr);
      auto packet = PacketPairInit();
      packet.protoVersion = json["protoVersion"];
      try {
        packet.deviceUUID = json["deviceUUID"];
        packet.deviceName = json["deviceName"];
        packet.ipAddress = json["ipAddress"];
        packet.tcpPort = json["tcpPort"];
        packet.udpPort = json["udpPort"];
        packet.udpManualPort = json["udpManualPort"];
        packet.cloudToken = json["cloudToken"];
      } catch(...) {
      }
      return packet;
    } catch(...) {
    }
    return {};
  }
};

struct PacketPairResponseData { // From PC
  PairingMethod pairingMethod{};
  std::string deviceId{};
  std::string deviceName{};
  std::string deviceOS{};
  std::string ipAddress{};
  uint16_t port{};
  std::string macAddress{};
  std::string userName{};
  std::string passwordKey{};

  nlohmann::json ToJson() {
    return {{"pairingMethod", PairingMethodUtils::ToString(pairingMethod)},
            {"deviceId", deviceId},
            {"deviceName", deviceName},
            {"deviceOS", deviceOS},
            {"ipAddress", ipAddress},
            {"port", port},
            {"macAddress", macAddress},
            {"userName", userName},
            {"passwordKey", passwordKey}};
  }
};

struct PacketPairResponse { // From PC
  std::string errMsg{};
  PacketPairResponseData data{};

  nlohmann::json ToJson() {
    return {{"errMsg", errMsg}, {"data", data.ToJson()}};
  }
};

struct PacketUnlockRequest {
  std::string protoVersion;
  std::string deviceId;
  std::string encData;

  nlohmann::json ToJson() {
    return {{"protoVersion", protoVersion}, {"deviceId", deviceId}, {"encData", encData}};
  }
};

struct PacketUnlockRequestData {
  std::string user;
  std::string program;
  std::string unlockToken;

  nlohmann::json ToJson() {
    return {{"user", user}, {"program", program}, {"unlockToken", unlockToken}};
  }
};

struct PacketUnlockResponse {
  std::string error;
  std::string encData;

  static std::optional<PacketUnlockResponse> FromJson(const std::string &jsonStr) {
    try {
      auto json = nlohmann::json::parse(jsonStr);
      auto packet = PacketUnlockResponse();
      packet.error = json["error"];
      packet.encData = json["encData"];
      return packet;
    } catch(...) {
    }
    return {};
  }
};

struct PacketUnlockResponseData {
  std::string unlockToken;
  std::string passwordKey;

  static std::optional<PacketUnlockResponseData> FromJson(const std::string &jsonStr) {
    try {
      auto json = nlohmann::json::parse(jsonStr);
      auto packet = PacketUnlockResponseData();
      packet.unlockToken = json["unlockToken"];
      packet.passwordKey = json["passwordKey"];
      return packet;
    } catch(...) {
    }
    return {};
  }
};

struct PacketUDPBroadcast {
  std::string deviceId;
  std::string pcbuIP;
  uint16_t pcbuPort;
  bool isManual;

  nlohmann::json ToJson() {
    return {{"deviceId", deviceId}, {"pcbuIP", pcbuIP}, {"pcbuPort", pcbuPort}, {"isManual", isManual}};
  }
};

#endif // PCBU_DESKTOP_PACKETS_H
