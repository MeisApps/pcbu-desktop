#ifndef PCBU_DESKTOP_PAIRINGMETHOD_H
#define PCBU_DESKTOP_PAIRINGMETHOD_H

#include <spdlog/spdlog.h>
#include <string>

enum class PairingMethod : int { TCP, BLUETOOTH, CLOUD, UDP, MANUAL_UDP };

class PairingMethodUtils {
public:
  static std::string ToString(PairingMethod method) {
    if(method == PairingMethod::TCP)
      return "TCP";
    else if(method == PairingMethod::BLUETOOTH)
      return "BLUETOOTH";
    else if(method == PairingMethod::CLOUD)
      return "CLOUD";
    else if(method == PairingMethod::UDP)
      return "UDP";
    else if(method == PairingMethod::MANUAL_UDP)
      return "MANUAL_UDP";
    spdlog::warn("Unknown pairing method.");
    return {};
  }

  static PairingMethod FromString(const std::string &methodStr) {
    if(methodStr == "TCP")
      return PairingMethod::TCP;
    else if(methodStr == "BLUETOOTH")
      return PairingMethod::BLUETOOTH;
    else if(methodStr == "CLOUD")
      return PairingMethod::CLOUD;
    else if(methodStr == "UDP")
      return PairingMethod::UDP;
    else if(methodStr == "MANUAL_UDP")
      return PairingMethod::MANUAL_UDP;
    spdlog::warn("Invalid pairing method '{}'.", methodStr);
    return PairingMethod::TCP;
  }

private:
  PairingMethodUtils() = default;
};

#endif // PCBU_DESKTOP_PAIRINGMETHOD_H
