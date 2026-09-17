#include "PairedDevicesStorage.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <vector>

#include "AppSettings.h"
#include "shell/Shell.h"
#include "utils/StringUtils.h"

std::vector<PairedDevice> PairedDevicesStorage::g_Cache{};
std::mutex PairedDevicesStorage::g_Mutex{};

std::optional<PairedDevice> PairedDevicesStorage::GetDeviceByID(const std::string &id) {
  for(const auto &device : GetDevices())
    if(device.id == id)
      return device;
  return {};
}

std::vector<PairedDevice> PairedDevicesStorage::GetDevicesForUser(const std::string &userName) {
  std::vector<PairedDevice> result{};
#ifdef WINDOWS
  auto userNameLower = StringUtils::ToLower(userName);
  std::string principalName{};
  auto userSplit = StringUtils::Split(userName, "\\");
  if(userSplit.size() == 2 && userSplit[1].find('@') != std::string::npos)
    principalName = StringUtils::ToLower(userSplit[1]);
#endif
  for(const auto &device : GetDevices()) {
#ifdef WINDOWS
    auto deviceUserName = StringUtils::ToLower(device.userName);
    if(deviceUserName == userNameLower || (!principalName.empty() && deviceUserName == principalName))
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
  std::unique_lock lock(g_Mutex);
  if(!g_Cache.empty())
    return g_Cache;
  g_Cache = Load();
  return g_Cache;
}

std::vector<PairedDevice> PairedDevicesStorage::Load() {
  auto filePath = AppSettings::GetBaseDir() / DEVICES_FILE_NAME;
#ifdef WINDOWS
  Shell::ProtectFile(filePath, false);
#endif
  auto jsonData = Shell::ReadBytes(filePath);
#ifdef WINDOWS
  Shell::ProtectFile(filePath, true);
#endif

  std::vector<PairedDevice> result{};
  if(jsonData.empty())
    return result;

  try {
    auto json = nlohmann::json::parse(jsonData);
    for(auto entry : json) {
      try {
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

        if(entry.contains("sshServers") && entry["sshServers"].is_array()) {
          for(const auto &sshEntry : entry["sshServers"]) {
            if(!sshEntry.is_object())
              continue;
            auto server = SshServer();
            server.host = sshEntry.value("host", "");
            server.user = sshEntry.value("user", "");
            server.keyPath = sshEntry.value("keyPath", "");
            server.passwordEnc = sshEntry.value("passwordEnc", "");
            if(server.host.empty() && server.keyPath.empty())
              continue;
            device.sshServers.emplace_back(server);
          }
        }
        result.emplace_back(device);
      } catch(const std::exception &ex) {
        spdlog::error("Skipped invalid paired device.");
      }
    }
  } catch(const std::exception &ex) {
    spdlog::error("Failed reading paired devices storage: {}", ex.what());
  }
  return result;
}

void PairedDevicesStorage::SaveDevices(const std::vector<PairedDevice> &devices) {
  std::unique_lock lock(g_Mutex);
  try {
    nlohmann::json devicesJson{};
    for(auto device : devices) {
      auto sshServersJson = nlohmann::json::array();
      for(const auto &server : device.sshServers)
        sshServersJson.push_back({{"host", server.host}, {"user", server.user}, {"keyPath", server.keyPath}, {"passwordEnc", server.passwordEnc}});

      nlohmann::json deviceJson = {
          {"id", device.id},
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
          {"cloudToken", device.cloudToken},

          {"sshServers", sshServersJson},
      };
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
    g_Cache = devices;
  } catch(const std::exception &ex) {
    spdlog::error("Failed writing paired devices storage: {}", ex.what());
  }
}

void PairedDevicesStorage::InvalidateCache() {
  std::unique_lock lock(g_Mutex);
  g_Cache = {};
}
