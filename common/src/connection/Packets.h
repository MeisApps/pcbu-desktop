#ifndef PCBU_DESKTOP_PACKETS_H
#define PCBU_DESKTOP_PACKETS_H

#include <cstdint>
#include <nlohmann/json.hpp>

#include "storage/PairingMethod.h"

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
  std::string deviceId;
  std::string encData;

  nlohmann::json ToJson() {
    return {{"deviceId", deviceId}, {"encData", encData}};
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
  std::string unlockToken;
  std::string passwordKey;

  static std::optional<PacketUnlockResponse> FromJson(const std::string &jsonStr) {
    try {
      auto json = nlohmann::json::parse(jsonStr);
      auto packet = PacketUnlockResponse();
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
