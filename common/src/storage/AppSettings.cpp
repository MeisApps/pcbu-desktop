#include "AppSettings.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "shell/Shell.h"
#include "utils/AppInfo.h"
#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <ShlObj_core.h>
#include <spdlog/fmt/xchar.h>
#endif

PCBUAppStorage AppSettings::g_Cache{};
std::mutex AppSettings::g_Mutex{};

std::filesystem::path AppSettings::GetBaseDir() {
#ifdef WINDOWS
  wchar_t szPath[MAX_PATH]{};
  if(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, szPath) == S_OK)
    return fmt::format(L"{}\\PCBioUnlock", szPath);
  return "C:\\ProgramData\\PCBioUnlock";
#else
  return {"/etc/pc-bio-unlock"};
#endif
}

PCBUAppStorage AppSettings::Get() {
  std::unique_lock lock(g_Mutex);
  if(!g_Cache.machineID.empty())
    return g_Cache;
  g_Cache = Load();
  return g_Cache;
}

PCBUAppStorage AppSettings::Load() {
  std::string machineID{};
  try {
    auto jsonData = Shell::ReadBytes(GetBaseDir() / SETTINGS_FILE_NAME);
    auto json = nlohmann::json::parse(jsonData);
    auto settings = PCBUAppStorage();
    bool save = false;
    try {
      machineID = json["machineID"];
    } catch(...) {
      machineID = StringUtils::RandomString(32);
      save = true;
    }
    settings.machineID = machineID;
    settings.installedVersion = json["installedVersion"];
    settings.language = json["language"];
    settings.serverIP = json["serverIP"];
    settings.serverMAC = json["serverMAC"];
    settings.pairingServerPort = json["pairingServerPort"];
    settings.unlockServerPort = json["unlockServerPort"];
    settings.clientSocketTimeout = json["clientSocketTimeout"];
    settings.clientConnectTimeout = json["clientConnectTimeout"];
    settings.clientConnectRetries = json["clientConnectRetries"];

    settings.winWaitForKeyPress = json["winWaitForKeyPress"];
    settings.winHidePasswordField = json["winHidePasswordField"];
    settings.unixSetPasswordPAM = json["unixSetPasswordPAM"];
    if(save) {
      g_Mutex.unlock();
      Save(settings);
      g_Mutex.lock();
    }
    return settings;
  } catch(const std::exception &ex) {
    spdlog::error("Failed reading app storage: {}", ex.what());
    auto def = PCBUAppStorage();
    def.machineID = machineID.empty() ? StringUtils::RandomString(32) : machineID;
    def.language = "auto";
    def.serverIP = "auto";
    def.pairingServerPort = 43295;
    def.unlockServerPort = 43296;
    def.clientSocketTimeout = 120;
    def.clientConnectTimeout = 5;
    def.clientConnectRetries = 2;

    def.winWaitForKeyPress = true;
    def.winHidePasswordField = false;
    def.unixSetPasswordPAM = false;
    g_Mutex.unlock();
    Save(def);
    g_Mutex.lock();
    return def;
  }
}

void AppSettings::Save(const PCBUAppStorage &storage) {
  std::unique_lock lock(g_Mutex);
  try {
    nlohmann::json json = {
        {"machineID", storage.machineID},
        {"installedVersion", storage.installedVersion},
        {"language", storage.language},
        {"serverIP", storage.serverIP},
        {"serverMAC", storage.serverMAC},
        {"pairingServerPort", storage.pairingServerPort},
        {"unlockServerPort", storage.unlockServerPort},
        {"clientSocketTimeout", storage.clientSocketTimeout},
        {"clientConnectTimeout", storage.clientConnectTimeout},
        {"clientConnectRetries", storage.clientConnectRetries},

        {"winWaitForKeyPress", storage.winWaitForKeyPress},
        {"winHidePasswordField", storage.winHidePasswordField},
        {"unixSetPasswordPAM", storage.unixSetPasswordPAM},
    };
    auto baseDir = GetBaseDir();
    if(!std::filesystem::exists(baseDir))
      Shell::CreateDir(baseDir);
    auto jsonStr = json.dump();
    Shell::WriteBytes(baseDir / SETTINGS_FILE_NAME, {jsonStr.begin(), jsonStr.end()});
    g_Cache = storage;
  } catch(const std::exception &ex) {
    spdlog::error("Failed writing app storage: {}", ex.what());
  }
}

void AppSettings::InvalidateCache() {
  std::unique_lock lock(g_Mutex);
  g_Cache = {};
}

void AppSettings::SetInstalledVersion(bool isInstalled) {
  auto settings = Get();
  settings.installedVersion = isInstalled ? AppInfo::GetVersion() : "";
  Save(settings);
  InvalidateCache();
}
