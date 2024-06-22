#ifndef PCBU_DESKTOP_PAIREDDEVICESSTORAGE_H
#define PCBU_DESKTOP_PAIREDDEVICESSTORAGE_H

#include <optional>
#include <string>
#include <vector>

#include "PairingMethod.h"

struct PairedDevice {
    std::string pairingId{};
    PairingMethod pairingMethod{};
    std::string deviceName{};
    std::string userName{};
    std::string encryptionKey{};

    std::string ipAddress{};
    std::string bluetoothAddress{};
    std::string cloudToken{};
};

class PairedDevicesStorage {
public:
    static std::optional<PairedDevice> GetDeviceByID(const std::string& pairingId);
    static std::vector<PairedDevice> GetDevicesForUser(const std::string& userName);

    static void AddDevice(const PairedDevice& device);
    static void RemoveDevice(const std::string& pairingId);

    static std::vector<PairedDevice> GetDevices();
    static void SaveDevices(const std::vector<PairedDevice>& devices);

    static void ProtectFile(const std::string& filePath, bool protect);

private:
    static constexpr std::string_view DEVICES_FILE_NAME = "paired_devices.json";
};


#endif //PCBU_DESKTOP_PAIREDDEVICESSTORAGE_H
