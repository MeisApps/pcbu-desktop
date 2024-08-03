#ifndef PCBU_DESKTOP_PAIRINGSTRUCTS_H
#define PCBU_DESKTOP_PAIRINGSTRUCTS_H

#include <string>

#include "storage/PairedDevicesStorage.h"

struct PacketPairInit { // From phone
    std::string protoVersion{};
    std::string deviceUUID{};
    std::string deviceName{};
    std::string ipAddress{};
    std::string cloudToken{};

    static std::optional<PacketPairInit> FromJson(const std::string& jsonStr) {
        try {
            auto json = nlohmann::json::parse(jsonStr);
            auto packet = PacketPairInit();
            packet.protoVersion = json["protoVersion"];
            packet.deviceUUID = json["deviceUUID"];
            packet.deviceName = json["deviceName"];
            packet.ipAddress = json["ipAddress"];
            packet.cloudToken = json["cloudToken"];
            return packet;
        } catch(...) {}
        return {};
    }
};

struct PacketPairResponse { // From PC
    std::string errMsg{};
    std::string pairingId{};
    PairingMethod pairingMethod{};
    std::string hostName{};
    std::string hostOS{};
    std::string hostAddress{};
    uint16_t hostPort{};
    std::string macAddress{};
    std::string userName{};
    std::string password{};

    nlohmann::json ToJson() {
        return {
                {"errMsg", errMsg},
                {"pairingId", pairingId},
                {"pairingMethod", PairingMethodUtils::ToString(pairingMethod)},
                {"hostName", hostName},
                {"hostOS", hostOS},
                {"hostAddress", hostAddress},
                {"hostPort", hostPort},
                {"macAddress", macAddress},
                {"userName", userName},
                {"password", password}
        };
    }
};

struct PairingQRData {
    std::string ip{};
    uint16_t port{};
    PairingMethod method{};
    std::string encKey{};

    PairingQRData(const std::string& ip, uint16_t port, PairingMethod pairingMethod, const std::string& encKey) {
        this->ip = ip;
        this->port = port;
        this->method = pairingMethod;
        this->encKey = encKey;
    }

    nlohmann::json ToJson() {
        return {
            {"ip", ip},
            {"port", port},
            {"method", method},
            {"encKey", encKey}
        };
    }
};

struct PairingServerData {
    std::string userName{};
    std::string password{};
    std::string encKey{};
    PairingMethod method{};
    std::string macAddress{};
    std::string btAddress{};
};

#endif //PCBU_DESKTOP_PAIRINGSTRUCTS_H
