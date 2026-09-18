#include "AppSettings.h"

#include <filesystem>
#include <stdexcept>

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
#include <unistd.h>
#endif

PCBUAppStorage AppSettings::g_Cache{};
std::mutex AppSettings::g_Mutex{};

std::filesystem::path AppSettings::GetBaseDir() {
  auto newDir = GetNewBaseDir();
  auto oldDir = GetOldBaseDir();
  if(std::filesystem::exists(newDir / SETTINGS_FILE_NAME))
    return newDir;
  if(std::filesystem::exists(oldDir / SETTINGS_FILE_NAME))
    return oldDir;
  return newDir;
}

std::filesystem::path AppSettings::GetBaseUserDir() {
#ifdef WINDOWS
  wchar_t szPath[MAX_PATH]{};
  if(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, szPath) == S_OK)
    return std::filesystem::path(fmt::format(L"{}\\PulseUnlock", szPath));
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
    return std::filesystem::path(homeDir) / ".local/share/PulseUnlock";
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
    settings.pairingDiscoveryPort = json.value("pairingDiscoveryPort", defaults.pairingDiscoveryPort);
    settings.pairingServerPort = json.value("pairingServerPort", defaults.pairingServerPort);
    settings.unlockServerPort = json.value("unlockServerPort", defaults.unlockServerPort);
    settings.clientSocketTimeout = json.value("clientSocketTimeout", defaults.clientSocketTimeout);
    settings.clientConnectTimeout = json.value("clientConnectTimeout", defaults.clientConnectTimeout);
    settings.clientConnectRetries = json.value("clientConnectRetries", defaults.clientConnectRetries);

    settings.winUnlockBehavior = json.value("winUnlockBehavior", defaults.winUnlockBehavior);
    settings.winHidePasswordField = json.value("winHidePasswordField", defaults.winHidePasswordField);
    settings.winForceDefaultCredProv = json.value("winForceDefaultCredProv", defaults.winForceDefaultCredProv);
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
  def.pairingDiscoveryPort = 43297;
  def.pairingServerPort = 43295;
  def.unlockServerPort = 43296;
  def.clientSocketTimeout = 120;
  def.clientConnectTimeout = 5;
  def.clientConnectRetries = 2;

  def.winUnlockBehavior = "key_press_lock_only";
  def.winHidePasswordField = false;
  def.winForceDefaultCredProv = true;
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
        {"pairingDiscoveryPort", storage.pairingDiscoveryPort},
        {"pairingServerPort", storage.pairingServerPort},
        {"unlockServerPort", storage.unlockServerPort},
        {"clientSocketTimeout", storage.clientSocketTimeout},
        {"clientConnectTimeout", storage.clientConnectTimeout},
        {"clientConnectRetries", storage.clientConnectRetries},

        {"winUnlockBehavior", storage.winUnlockBehavior},
        {"winHidePasswordField", storage.winHidePasswordField},
        {"winForceDefaultCredProv", storage.winForceDefaultCredProv},
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

/*
 * Migration
 */

std::filesystem::path AppSettings::GetNewBaseDir() {
#ifdef WINDOWS
  wchar_t szPath[MAX_PATH]{};
  if(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, szPath) == S_OK)
    return fmt::format(L"{}\\PulseUnlock", szPath);
  return "C:\\ProgramData\\PulseUnlock";
#else
  return {"/etc/pulse-unlock"};
#endif
}

std::filesystem::path AppSettings::GetOldBaseDir() {
#ifdef WINDOWS
  wchar_t szPath[MAX_PATH]{};
  if(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, szPath) == S_OK)
    return fmt::format(L"{}\\PCBioUnlock", szPath);
  return "C:\\ProgramData\\PCBioUnlock";
#else
  return {"/etc/pc-bio-unlock"};
#endif
}

bool AppSettings::NeedsMigration() {
  auto newDir = GetNewBaseDir();
  auto oldDir = GetOldBaseDir();
  if(std::filesystem::exists(newDir / SETTINGS_FILE_NAME))
    return false;
  if(!std::filesystem::exists(oldDir / SETTINGS_FILE_NAME))
    return false;
  return true;
}

void AppSettings::MigrateBaseDir() {
  if(!NeedsMigration())
    return;

  auto newDir = GetNewBaseDir();
  auto oldDir = GetOldBaseDir();
  auto oldDevicesFile = oldDir / "paired_devices.json";
  auto newDevicesFile = newDir / "paired_devices.json";
  Shell::ProtectFile(oldDevicesFile, false);
  try {
#ifdef WINDOWS
    auto cmd = fmt::format(R"(move "{}" "{}")", oldDir.string(), newDir.string());
#else
    auto cmd = fmt::format(R"(mv "{}" "{}")", oldDir.string(), newDir.string());
#endif
    auto result = Shell::RunCommand(cmd);
    if(result.exitCode != 0)
      throw std::runtime_error(fmt::format("Migration rename failed. (Code={}, Output={})", result.exitCode, StringUtils::Trim(result.output)));
  } catch(...) {
    Shell::ProtectFile(oldDevicesFile, true);
    throw;
  }
  Shell::ProtectFile(newDevicesFile, true);
}
