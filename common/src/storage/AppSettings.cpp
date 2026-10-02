#include "AppSettings.h"

#include <filesystem>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "platform/PlatformHelper.h"
#include "shell/Shell.h"
#include "utils/AppInfo.h"
#include "utils/StringUtils.h"

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
  auto dataDir = PlatformHelper::GetUserDataDir();
  if(dataDir.empty())
    return PlatformHelper::GetTempDir();
  return dataDir / "PulseUnlock";
}

std::filesystem::path AppSettings::GetLogsDir(bool userDir) {
  auto logsDir = userDir ? PlatformHelper::GetUserLogsDir() : PlatformHelper::GetSystemLogsDir();
  if(logsDir.empty())
    return PlatformHelper::GetTempDir() / "logs";
#ifdef WINDOWS
  return logsDir / "PulseUnlock" / "logs";
#elif APPLE
  return logsDir / "PulseUnlock";
#else
  return userDir ? logsDir / "PulseUnlock" / "logs" : logsDir / "pulse-unlock";
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
  auto defaults = LoadDefaults();
  auto settingsPath = GetBaseDir() / SETTINGS_FILE_NAME;
  if(!std::filesystem::exists(settingsPath)) {
    Write(defaults);
    return defaults;
  }
  auto jsonData = Shell::ReadBytes(settingsPath);
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
    if(!json.contains("machineID"))
      Write(settings);
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
  Write(storage);
  g_Cache = storage;
}

void AppSettings::Write(const PCBUAppStorage &storage) {
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
  auto dataDir = PlatformHelper::GetSystemDataDir();
  if(dataDir.empty())
    return "C:\\ProgramData\\PulseUnlock";
  return dataDir / "PulseUnlock";
#else
  return PlatformHelper::GetSystemDataDir() / "pulse-unlock";
#endif
}

std::filesystem::path AppSettings::GetOldBaseDir() {
#ifdef WINDOWS
  auto dataDir = PlatformHelper::GetSystemDataDir();
  if(dataDir.empty())
    return "C:\\ProgramData\\PCBioUnlock";
  return dataDir / "PCBioUnlock";
#else
  return PlatformHelper::GetSystemDataDir() / "pc-bio-unlock";
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
  if(std::filesystem::exists(newDir)) {
    auto logsDir = GetLogsDir(false);
    std::vector<std::filesystem::path> entries{};
    for(const auto &entry : std::filesystem::directory_iterator(newDir))
      entries.push_back(entry.path());
    for(const auto &entry : entries) {
      if(entry != logsDir || std::filesystem::exists(oldDir / entry.filename()))
        throw std::runtime_error(fmt::format("Migration target '{}' already exists.", entry.string()));
    }
    for(const auto &entry : entries)
      MovePath(entry, oldDir / entry.filename());
    if(!Shell::Remove(newDir))
      throw std::runtime_error(fmt::format("Failed removing empty migration target '{}'.", newDir.string()));
  }
  Shell::ProtectFile(oldDevicesFile, false);
  try {
    MovePath(oldDir, newDir);
  } catch(...) {
    Shell::ProtectFile(oldDevicesFile, true);
    throw;
  }
  if(std::filesystem::exists(newDevicesFile) && !Shell::ProtectFile(newDevicesFile, true))
    spdlog::warn("Failed setting permissions of '{}'.", newDevicesFile.string());
}

void AppSettings::MovePath(const std::filesystem::path &from, const std::filesystem::path &to) {
#ifdef WINDOWS
  auto cmd = fmt::format(R"(move "{}" "{}")", from.string(), to.string());
#else
  auto cmd = fmt::format(R"(mv "{}" "{}")", from.string(), to.string());
#endif
  auto result = Shell::RunCommand(cmd);
  if(result.exitCode != 0)
    throw std::runtime_error(fmt::format("Migration move failed. (From={}, To={}, Code={}, Output={})", from.string(), to.string(), result.exitCode,
                                         StringUtils::Trim(result.output)));
}
