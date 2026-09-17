#include "ServiceInstaller.h"

#include <Windows.h>

#include <ShlObj.h>

#include "WinFirewallHelper.h"
#include "platform/PlatformHelper.h"
#include "shell/Shell.h"
#include "storage/AppSettings.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/ResourceHelper.h"
#include "utils/StringUtils.h"

#define LIB_MODULE_FILE "win_pulseunlock.dll"
#define LIB_MODULE_FILE_OLD "win-pcbiounlock.dll"
#define CRED_PROVIDER_NAME "win_pulseunlock"
#define CRED_PROVIDER_GUID "{74A23DE2-B81D-46EC-E129-CD32507ED716}"
#define APP_FIREWALL_RULE_NAME "PulseUnlock"
#define APP_FIREWALL_RULE_NAME_OLD "PC Bio Unlock"
#define LOGONUI_FIREWALL_RULE_NAME "LogonUI (PulseUnlock)"
#define LOGONUI_FIREWALL_RULE_NAME_OLD "LogonUI (PC Bio Unlock)"

std::filesystem::path ServiceInstaller_GetSysDir() {
  wchar_t sysDir[MAX_PATH]{};
  if(GetSystemDirectoryW(sysDir, MAX_PATH) > 0)
    return sysDir;
  return "C:\\Windows\\System32";
}

ServiceInstaller::ServiceInstaller(const std::function<void(const std::string &)> &logCallback) {
  m_Logger = logCallback;
}

std::vector<ServiceSetting> ServiceInstaller::GetSettings() {
  return {
      {"unlockBehavior",
       I18n::Get("service_setting_unlock_behavior"),
       {
           {"foreground_lock_only", I18n::Get("unlock_behavior_foreground_lock_only")},
           {"foreground_always", I18n::Get("unlock_behavior_foreground_always")},
           {"key_press_lock_only", I18n::Get("unlock_behavior_key_press_lock_only")},
           {"key_press", I18n::Get("unlock_behavior_key_press")},
           {"none", I18n::Get("unlock_behavior_none")},
       },
       AppSettings::Get().winUnlockBehavior,
       "key_press_lock_only"},
      {"hidePasswordField", I18n::Get("service_setting_hide_pw_field"), AppSettings::Get().winHidePasswordField, false},
      {"forceCredProv", I18n::Get("service_setting_force_cred_prov"), AppSettings::Get().winForceDefaultCredProv, true},
  };
}

void ServiceInstaller::ApplySettings(const std::vector<ServiceSetting> &settings, bool useDefault) {
  for(auto setting : settings) {
    if(setting.id == "unlockBehavior") {
      auto storage = AppSettings::Get();
      storage.winUnlockBehavior = useDefault ? setting.defaultValue : setting.selectedValue;
      AppSettings::Save(storage);
    } else if(setting.id == "hidePasswordField") {
      auto storage = AppSettings::Get();
      storage.winHidePasswordField = useDefault ? setting.defaultVal : setting.enabled;
      AppSettings::Save(storage);
    } else if(setting.id == "forceCredProv") {
      auto storage = AppSettings::Get();
      storage.winForceDefaultCredProv = useDefault ? setting.defaultVal : setting.enabled;
      AppSettings::Save(storage);
    } else {
      spdlog::warn("Unknown service setting {}.", setting.id);
    }
  }
}

bool ServiceInstaller::IsInstalled() {
  auto sysDir = ServiceInstaller_GetSysDir();
  return std::filesystem::exists(sysDir / LIB_MODULE_FILE) || std::filesystem::exists(sysDir / LIB_MODULE_FILE_OLD);
}

void ServiceInstaller::Install() {
  m_Logger("Copying credential provider...");
  auto nativeLib = ResourceHelper::GetResource(":/res/natives/{}", LIB_MODULE_FILE);
  auto sysDir = ServiceInstaller_GetSysDir();
  auto libPath = sysDir / LIB_MODULE_FILE;
  auto result = Shell::WriteBytes(libPath, nativeLib);
  if(!result)
    throw std::runtime_error(I18n::Get("error_file_write", libPath.string()));

  // Migration
  auto oldLibPath = sysDir / LIB_MODULE_FILE_OLD;
  if(std::filesystem::exists(oldLibPath)) {
    result = Shell::RemoveFile(oldLibPath);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_remove", oldLibPath.string()));
  }

  m_Logger("Creating registry entries...");
  result =
      Shell::RunCommand(
          fmt::format(
              R"(reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{0}" /t REG_SZ /d {1} /f)",
              CRED_PROVIDER_GUID, CRED_PROVIDER_NAME))
              .exitCode == 0 &&
      Shell::RunCommand(fmt::format(R"(reg add "HKEY_CLASSES_ROOT\CLSID\{0}" /t REG_SZ /d {1} /f)", CRED_PROVIDER_GUID, CRED_PROVIDER_NAME))
              .exitCode == 0 &&
      Shell::RunCommand(
          fmt::format(R"(reg add "HKEY_CLASSES_ROOT\CLSID\{0}\InprocServer32" /t REG_SZ /d {1} /f)", CRED_PROVIDER_GUID, CRED_PROVIDER_NAME))
              .exitCode == 0 &&
      Shell::RunCommand(
          fmt::format(R"(reg add "HKEY_CLASSES_ROOT\CLSID\{0}\InprocServer32" /t REG_SZ /v ThreadingModel /d Apartment /f)", CRED_PROVIDER_GUID))
              .exitCode == 0;
  if(!result)
    throw std::runtime_error(I18n::Get("error_registry_add"));

  wchar_t wExePath[MAX_PATH]{};
  if(GetModuleFileNameW(nullptr, wExePath, MAX_PATH) > 0 && std::filesystem::exists(wExePath)) {
    auto exePath = StringUtils::FromWideString(wExePath);
    m_Logger("Removing old firewall rules for PulseUnlock...");
    WinFirewallHelper::RemoveAllRulesForProgram(exePath);

    // Migration
    wchar_t programFiles[MAX_PATH]{};
    if(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, programFiles) == S_OK) {
      auto oldInstallExe = std::filesystem::path(programFiles) / "PCBioUnlock" / "pcbu_desktop.exe";
      if(std::filesystem::exists(oldInstallExe) && oldInstallExe != std::filesystem::path(wExePath))
        WinFirewallHelper::RemoveAllRulesForProgram(oldInstallExe.string());
    }

    m_Logger("Adding Windows firewall rule for PulseUnlock...");
    result = Shell::RunCommand(fmt::format(R"(netsh advfirewall firewall add rule name="{0}" dir=in program="{1}" profile=any action=allow)",
                                           APP_FIREWALL_RULE_NAME, exePath))
                 .exitCode == 0;
    if(!result)
      m_Logger(I18n::Get("error_firewall_rule_add", "Windows Firewall (PulseUnlock)"));
  } else {
    m_Logger("Warning: App path not found. Skipped adding firewall rule.");
  }

  auto logonUiPath = ServiceInstaller_GetSysDir() / "LogonUI.exe";
  m_Logger("Removing old firewall rules for LogonUI...");
  WinFirewallHelper::RemoveAllRulesForProgram(logonUiPath.string());
  m_Logger("Adding Windows firewall rule for LogonUI...");
  result = Shell::RunCommand(fmt::format(R"(netsh advfirewall firewall add rule name="{0}" dir=in program="{1}" profile=any action=allow)",
                                         LOGONUI_FIREWALL_RULE_NAME, logonUiPath.string()))
               .exitCode == 0;
  if(!result)
    m_Logger(I18n::Get("error_firewall_rule_add", "Windows Firewall (LogonUI)"));

  m_Logger("Setting default credential provider...");
  for(auto device : PairedDevicesStorage::GetDevices()) {
    if(!PlatformHelper::SetDefaultCredProv(device.userName, CRED_PROVIDER_GUID))
      m_Logger(fmt::format("Failed setting default credential provider for user '{}'.", device.userName));
  }
  m_Logger("Done.");
}

void ServiceInstaller::Uninstall(bool /*fullUninstall*/) {
  m_Logger("Removing credential provider...");
  auto sysDir = ServiceInstaller_GetSysDir();
  auto result = true;
  for(const auto &moduleFile : {LIB_MODULE_FILE, LIB_MODULE_FILE_OLD}) {
    auto libPath = sysDir / moduleFile;
    if(!std::filesystem::exists(libPath))
      continue;
    result = Shell::RemoveFile(libPath);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_remove", libPath.string()));
  }

  m_Logger("Removing registry entries...");
  result = Shell::RunCommand(
               fmt::format(R"(reg delete "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{0}" /f)",
                           CRED_PROVIDER_GUID))
                   .exitCode == 0 &&
           Shell::RunCommand(fmt::format(R"(reg delete "HKEY_CLASSES_ROOT\CLSID\{0}" /f)", CRED_PROVIDER_GUID)).exitCode == 0;
  if(!result)
    throw std::runtime_error(I18n::Get("error_registry_remove"));

  m_Logger("Removing Windows firewall rules...");
  for(const auto &ruleName : {APP_FIREWALL_RULE_NAME, APP_FIREWALL_RULE_NAME_OLD, LOGONUI_FIREWALL_RULE_NAME, LOGONUI_FIREWALL_RULE_NAME_OLD}) {
    if(Shell::RunCommand(fmt::format(R"(netsh advfirewall firewall show rule name="{0}")", ruleName)).exitCode != 0)
      continue;
    result = Shell::RunCommand(fmt::format(R"(netsh advfirewall firewall delete rule name="{0}")", ruleName)).exitCode == 0;
    if(!result)
      m_Logger(I18n::Get("error_firewall_rule_remove", fmt::format("Windows Firewall ({})", ruleName)));
  }
  m_Logger("Done.");
}
