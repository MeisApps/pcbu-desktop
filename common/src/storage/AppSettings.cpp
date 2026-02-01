#include "AppSettings.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "shell/Shell.h"
#include "utils/AppInfo.h"

#ifdef WINDOWS
#include <ShlObj_core.h>
#include <spdlog/fmt/xchar.h>
#endif

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
  try {
    auto jsonData = Shell::ReadBytes(GetBaseDir() / SETTINGS_FILE_NAME);
    auto json = nlohmann::json::parse(jsonData);
    auto settings = PCBUAppStorage();
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
    return settings;
  } catch(const std::exception &ex) {
    spdlog::error("Failed reading app storage: {}", ex.what());
    auto def = PCBUAppStorage();
    def.language = "auto";
    def.serverIP = "auto";
    def.pairingServerPort = 43295;
    def.unlockServerPort = 43296;
    def.clientSocketTimeout = 120;
    def.clientConnectTimeout = 5;
    def.clientConnectRetries = 2;

    def.winWaitForKeyPress = true;
    def.winHidePasswordField = false;
    spdlog::info("Creating new app storage...");
    Save(def);
    return def;
  }
}

void AppSettings::Save(const PCBUAppStorage &storage) {
  try {
    nlohmann::json json = {
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
    };
    auto baseDir = GetBaseDir();
    if(!std::filesystem::exists(baseDir))
      Shell::CreateDir(baseDir);
    auto jsonStr = json.dump();
    Shell::WriteBytes(baseDir / SETTINGS_FILE_NAME, {jsonStr.begin(), jsonStr.end()});
  } catch(const std::exception &ex) {
    spdlog::error("Failed writing app storage: {}", ex.what());
  }
}

void AppSettings::SetInstalledVersion(bool isInstall) {
  auto settings = AppSettings::Get();
  settings.installedVersion = isInstall ? AppInfo::GetVersion() : "";
  AppSettings::Save(settings);
}
