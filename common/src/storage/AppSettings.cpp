#include "AppSettings.h"

#include <filesystem>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "shell/Shell.h"
#include "utils/AppInfo.h"
#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <ShlObj_core.h>
#include <spdlog/fmt/xchar.h>
#else
#include <pwd.h>
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
  return "/etc/pc-bio-unlock";
#endif
}

std::filesystem::path AppSettings::GetBaseUserDir() {
#ifdef WINDOWS
  wchar_t szPath[MAX_PATH]{};
  if(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, szPath) == S_OK)
    return fmt::format(L"{}\\PCBioUnlock", szPath);
  wchar_t tmp[MAX_PATH]{};
  GetTempPathW(MAX_PATH, tmp);
  return tmp;
#else
  auto homeDir = std::getenv("HOME");
  if(!homeDir) {
    uid_t uid = getuid();
    auto pw = getpwuid(uid);
    if(pw) {
      homeDir = pw->pw_dir;
    }
  }
  if(homeDir && homeDir[0] != '\0')
    return std::filesystem::path(homeDir) / ".local/share/PCBioUnlock";
  return "/tmp";
#endif
}

std::filesystem::path AppSettings::GetLogsDir(bool userDir) {
  if(userDir)
    return GetBaseUserDir() / "logs";
  return GetBaseDir() / "logs";
}

PCBUAppStorage AppSettings::Get() {
  std::unique_lock lock(g_Mutex);
  if(!g_Cache.machineID.empty())
    return g_Cache;
  g_Cache = Load();
  return g_Cache;
}

PCBUAppStorage AppSettings::Load() {
  auto defaults = LoadDefaults();
  auto jsonData = Shell::ReadBytes(GetBaseDir() / SETTINGS_FILE_NAME);
  if(jsonData.empty())
    return defaults;

  try {
    auto json = nlohmann::json::parse(jsonData);
    auto settings = PCBUAppStorage();
    settings.machineID = json.value("machineID", defaults.machineID);
    settings.installedVersion = json.value("installedVersion", defaults.installedVersion);
    settings.language = json.value("language", defaults.language);
    settings.serverIP = json.value("serverIP", defaults.serverIP);
    settings.serverMAC = json.value("serverMAC", defaults.serverMAC);
    settings.pairingServerPort = json.value("pairingServerPort", defaults.pairingServerPort);
    settings.unlockServerPort = json.value("unlockServerPort", defaults.unlockServerPort);
    settings.clientSocketTimeout = json.value("clientSocketTimeout", defaults.clientSocketTimeout);
    settings.clientConnectTimeout = json.value("clientConnectTimeout", defaults.clientConnectTimeout);
    settings.clientConnectRetries = json.value("clientConnectRetries", defaults.clientConnectRetries);

    settings.winWaitForKeyPress = json.value("winWaitForKeyPress", defaults.winWaitForKeyPress);
    settings.winHidePasswordField = json.value("winHidePasswordField", defaults.winHidePasswordField);
    settings.unixSetPasswordPAM = json.value("unixSetPasswordPAM", defaults.unixSetPasswordPAM);
    return settings;
  } catch(const std::exception &ex) {
    spdlog::error("Failed reading app storage: {}", ex.what());
    return LoadDefaults();
  }
}

PCBUAppStorage AppSettings::LoadDefaults() {
  auto def = PCBUAppStorage();
  def.machineID = StringUtils::RandomString(32);
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
  return def;
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
}
