#ifndef PCBU_DESKTOP_PAIRINGMETHOD_H
#define PCBU_DESKTOP_PAIRINGMETHOD_H

#include <string>
#include "spdlog/spdlog.h"

enum class PairingMethod : int {
    TCP,
    BLUETOOTH,
    CLOUD_TCP
};

class PairingMethodUtils {
public:
    static std::string ToString(PairingMethod method) {
        if(method == PairingMethod::TCP)
            return "TCP";
        else if(method == PairingMethod::BLUETOOTH)
            return "BLUETOOTH";
        else if(method == PairingMethod::CLOUD_TCP)
            return "CLOUD_TCP";
        spdlog::warn("Unknown pairing method.");
        return {};
    }

    static PairingMethod FromString(const std::string& methodStr) {
        if(methodStr == "TCP")
            return PairingMethod::TCP;
        else if(methodStr == "BLUETOOTH")
            return PairingMethod::BLUETOOTH;
        else if(methodStr == "CLOUD_TCP")
            return PairingMethod::CLOUD_TCP;
        spdlog::warn("Invalid pairing method '{}'.", methodStr);
        return PairingMethod::TCP;
    }

private:
    PairingMethodUtils() = default;
};

#endif //PCBU_DESKTOP_PAIRINGMETHOD_H
