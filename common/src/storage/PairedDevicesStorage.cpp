#include "PairedDevicesStorage.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "AppSettings.h"
#include "shell/Shell.h"

#ifdef APPLE
#define CHOWN_USER "root"
#elif LINUX
#define CHOWN_USER "root:root"
#endif

std::optional<PairedDevice> PairedDevicesStorage::GetDeviceByID(const std::string& pairingId) {
    for(const auto& device : GetDevices())
        if(device.pairingId == pairingId)
            return device;
    return {};
}

std::vector<PairedDevice> PairedDevicesStorage::GetDevicesForUser(const std::string &userName) {
    std::vector<PairedDevice> result{};
    for(const auto& device : GetDevices()) {
        if(device.userName == userName)
            result.emplace_back(device);
    }
    return result;
}

void PairedDevicesStorage::AddDevice(const PairedDevice &device) {
    auto devices = PairedDevicesStorage::GetDevices();
    auto rmRange = std::ranges::remove_if(devices, [&](const PairedDevice& d) {
        return d.pairingId == device.pairingId;
    });
    devices.erase(rmRange.begin(), rmRange.end());
    devices.emplace_back(device);
    PairedDevicesStorage::SaveDevices(devices);
}

void PairedDevicesStorage::RemoveDevice(const std::string &pairingId) {
    auto devices = PairedDevicesStorage::GetDevices();
    auto rmRange = std::ranges::remove_if(devices, [&](const PairedDevice& d) {
        return d.pairingId == pairingId;
    });
    devices.erase(rmRange.begin(), rmRange.end());
    PairedDevicesStorage::SaveDevices(devices);
}

std::vector<PairedDevice> PairedDevicesStorage::GetDevices() {
    std::vector<PairedDevice> result{};
    try {
        auto filePath = AppSettings::BASE_DIR / DEVICES_FILE_NAME;
#ifdef WINDOWS
        ProtectFile(filePath.string(), false);
#endif
        auto jsonData = Shell::ReadBytes(filePath);
#ifdef WINDOWS
        ProtectFile(filePath.string(), true);
#endif
        auto json = nlohmann::json::parse(jsonData);
        for(auto entry : json) {
            auto device = PairedDevice();
            device.pairingId = entry["pairingId"];
            device.pairingMethod = PairingMethodUtils::FromString(entry["pairingMethod"]);
            device.deviceName = entry["deviceName"];
            device.userName = entry["userName"];
            device.encryptionKey = entry["encryptionKey"];

            device.ipAddress = entry["ipAddress"];
            device.bluetoothAddress = entry["bluetoothAddress"];
            device.cloudToken = entry["cloudToken"];
            result.emplace_back(device);
        }
    } catch (const std::exception& ex) {
        spdlog::error("Failed reading paired devices storage: {}", ex.what());
        spdlog::info("Creating new devices storage...");
        SaveDevices({});
    }
    return result;
}

void PairedDevicesStorage::SaveDevices(const std::vector<PairedDevice> &devices) {
    try {
        nlohmann::json devicesJson{};
        for(auto device : devices) {
            nlohmann::json deviceJson = {
                    {"pairingId", device.pairingId},
                    {"pairingMethod", PairingMethodUtils::ToString(device.pairingMethod)},
                    {"deviceName", device.deviceName},
                    {"userName", device.userName},
                    {"encryptionKey", device.encryptionKey},

                    {"ipAddress", device.ipAddress},
                    {"bluetoothAddress", device.bluetoothAddress},
                    {"cloudToken", device.cloudToken}};
            devicesJson.emplace_back(deviceJson);
        }

        if(!std::filesystem::exists(AppSettings::BASE_DIR))
            Shell::CreateDir(AppSettings::BASE_DIR);
        auto filePath = AppSettings::BASE_DIR / DEVICES_FILE_NAME;
        ProtectFile(filePath.string(), false);
        auto jsonStr = devicesJson.dump();
        Shell::WriteBytes(filePath, {jsonStr.begin(), jsonStr.end()});
        ProtectFile(filePath.string(), true);
    } catch (const std::exception& ex) {
        spdlog::error("Failed writing paired devices storage: {}", ex.what());
    }
}

void PairedDevicesStorage::ProtectFile(const std::string &filePath, bool protect) {
    if(!std::filesystem::exists(filePath))
        return;
#ifdef WINDOWS
    auto cmd = Shell::RunCommand(fmt::format("icacls \"{}\" /{} *S-1-5-32-545:F", filePath, protect ? "deny" : "grant"));
    if(cmd.exitCode != 0)
        throw std::runtime_error(fmt::format("Error setting file permissions. (Code={}) {}", cmd.exitCode, cmd.output));
#elif defined(APPLE) || defined(LINUX)
    if(protect) {
        auto cmd = Shell::RunCommand(fmt::format(R"(chown {0} "{1}" && chmod 600 "{1}")", CHOWN_USER, filePath));
        if(cmd.exitCode != 0)
            throw std::runtime_error(fmt::format("Error setting file permissions. (Code={})", cmd.exitCode));
    }
#endif
}
