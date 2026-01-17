#ifndef PCBU_DESKTOP_PAIRINGSTRUCTS_H
#define PCBU_DESKTOP_PAIRINGSTRUCTS_H

#include <cstdint>
#include <nlohmann/json.hpp>

#include "storage/PairingMethod.h"

struct PairingQRData {
  std::string ip{};
  uint16_t port{};
  std::string method{};
  std::string encKey{};

  PairingQRData(const std::string &ip, uint16_t port, PairingMethod pairingMethod, const std::string &encKey) {
    this->ip = ip;
    this->port = port;
    this->method = PairingMethodUtils::ToString(pairingMethod);
    this->encKey = encKey;
  }

  nlohmann::json ToJson() {
    return {{"ip", ip}, {"port", port}, {"method", method}, {"encKey", encKey}};
  }
};

struct PairingUIData {
  std::string userName{};
  std::string password{};
  std::string encKey{};
  PairingMethod method{};
  std::string macAddress{};
  std::string btAddress{};
};

#endif // PCBU_DESKTOP_PAIRINGSTRUCTS_H
