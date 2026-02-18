#include "PairedDevicesStorage.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "AppSettings.h"
#include "shell/Shell.h"
#include "utils/StringUtils.h"

std::optional<PairedDevice> PairedDevicesStorage::GetDeviceByID(const std::string &id) {
  for(const auto &device : GetDevices())
    if(device.id == id)
      return device;
  return {};
}

std::vector<PairedDevice> PairedDevicesStorage::GetDevicesForUser(const std::string &userName) {
  std::vector<PairedDevice> result{};
  for(const auto &device : GetDevices()) {
#ifdef WINDOWS
    if(StringUtils::ToLower(device.userName) == StringUtils::ToLower(userName))
      result.emplace_back(device);
#else
    if(device.userName == userName)
      result.emplace_back(device);
#endif
  }
  return result;
}

void PairedDevicesStorage::AddDevice(const PairedDevice &device) {
  auto devices = PairedDevicesStorage::GetDevices();
  auto rmRange = std::ranges::remove_if(devices, [&](const PairedDevice &d) { return d.id == device.id; });
  devices.erase(rmRange.begin(), rmRange.end());
  devices.emplace_back(device);
  PairedDevicesStorage::SaveDevices(devices);
}

void PairedDevicesStorage::RemoveDevice(const std::string &id) {
  auto devices = PairedDevicesStorage::GetDevices();
  auto rmRange = std::ranges::remove_if(devices, [&](const PairedDevice &d) { return d.id == id; });
  devices.erase(rmRange.begin(), rmRange.end());
  PairedDevicesStorage::SaveDevices(devices);
}

std::vector<PairedDevice> PairedDevicesStorage::GetDevices() {
  std::vector<PairedDevice> result{};
  try {
    auto filePath = AppSettings::GetBaseDir() / DEVICES_FILE_NAME;
#ifdef WINDOWS
    Shell::ProtectFile(filePath, false);
#endif
    auto jsonData = Shell::ReadBytes(filePath);
#ifdef WINDOWS
    Shell::ProtectFile(filePath, true);
#endif
    auto json = nlohmann::json::parse(jsonData);
    for(auto entry : json) {
      auto device = PairedDevice();
      device.id = entry["id"];
      device.pairingMethod = PairingMethodUtils::FromString(entry["pairingMethod"]);
      device.deviceName = entry["deviceName"];
      device.userName = entry["userName"];
      device.passwordEnc = entry["passwordEnc"];
      device.encryptionKey = entry["encryptionKey"];

      device.ipAddress = entry["ipAddress"];
      device.bluetoothAddress = entry["bluetoothAddress"];
      device.cloudToken = entry["cloudToken"];
      device.tcpPort = entry["tcpPort"];
      device.udpPort = entry["udpPort"];
      device.udpManualPort = entry["udpManualPort"];
      result.emplace_back(device);
    }
  } catch(const std::exception &ex) {
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

      nlohmann::json deviceJson = {{"id", device.id},
                                   {"pairingMethod", PairingMethodUtils::ToString(device.pairingMethod)},
                                   {"deviceName", device.deviceName},
                                   {"userName", device.userName},
                                   {"passwordEnc", device.passwordEnc},
                                   {"encryptionKey", device.encryptionKey},

                                   {"ipAddress", device.ipAddress},
                                   {"tcpPort", device.tcpPort},
                                   {"udpPort", device.udpPort},
                                   {"udpManualPort", device.udpManualPort},
                                   {"bluetoothAddress", device.bluetoothAddress},
                                   {"cloudToken", device.cloudToken}};
      devicesJson.emplace_back(deviceJson);
    }
    auto baseDir = AppSettings::GetBaseDir();
    if(!std::filesystem::exists(baseDir))
      Shell::CreateDir(baseDir);
    auto filePath = baseDir / DEVICES_FILE_NAME;
    Shell::ProtectFile(filePath, false);
    auto jsonStr = devicesJson.dump();
    Shell::WriteBytes(filePath, {jsonStr.begin(), jsonStr.end()});
    Shell::ProtectFile(filePath, true);
  } catch(const std::exception &ex) {
    spdlog::error("Failed writing paired devices storage: {}", ex.what());
  }
}
