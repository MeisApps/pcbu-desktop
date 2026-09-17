#include "ServiceInstaller.h"

#include "EnvHelper.h"
#include "shell/Shell.h"
#include "utils/ResourceHelper.h"

#define EXE_MODULE_DIR std::filesystem::path("/usr/local/sbin/")
#define EXE_MODULE_FILE "pcbu_auth"
#define SSH_MODULE_FILE "pcbu_ssh_askpass"

#define PAM_MODULE_DIR std::filesystem::path("/usr/local/lib/pam/")
#define PAM_MODULE_FILE "pam_pulseunlock.dylib"
#define PAM_MODULE_FILE_OLD "pam_pcbiounlock.dylib"

#define PAM_CONFIG_ENTRY "auth sufficient /usr/local/lib/pam/pam_pulseunlock.dylib"
#define PAM_CONFIG_ENTRY_OLD "auth sufficient /usr/local/lib/pam/pam_pcbiounlock.dylib"

ServiceInstaller::ServiceInstaller(const std::function<void(const std::string &)> &logCallback) {
  m_Logger = logCallback;
  m_PAMHelper = PAMHelper(m_Logger);
}

std::vector<ServiceSetting> ServiceInstaller::GetSettings() {
  return {{"sudo", I18n::Get("service_setting_sudo"), PAMHelper::HasConfigEntry("sudo", PAM_CONFIG_ENTRY), true},
          {"macOS", I18n::Get("service_setting_macos"), PAMHelper::HasConfigEntry("authorization", PAM_CONFIG_ENTRY), false},
          {"ssh", I18n::Get("service_setting_ssh"), EnvHelper::IsSshEnabled(), false}};
}

void ServiceInstaller::ApplySettings(const std::vector<ServiceSetting> &settings, bool useDefault) {
  for(auto setting : settings) {
    auto isEnabled = useDefault ? setting.defaultVal : setting.enabled;
    if(setting.id == "sudo") {
      m_PAMHelper.SetConfigEntry("sudo", PAM_CONFIG_ENTRY, isEnabled);
    } else if(setting.id == "macOS") {
      m_PAMHelper.SetConfigEntry("authorization", PAM_CONFIG_ENTRY, isEnabled);
    } else if(setting.id == "ssh") {
      EnvHelper::SetSshEnabled(isEnabled);
    } else {
      spdlog::warn("Unknown service setting {}.", setting.id);
    }
  }
}

bool ServiceInstaller::IsInstalled() {
  auto hasAuth = std::filesystem::exists(EXE_MODULE_DIR / EXE_MODULE_FILE);
  auto hasPam = std::filesystem::exists(PAM_MODULE_DIR / PAM_MODULE_FILE) || std::filesystem::exists(PAM_MODULE_DIR / PAM_MODULE_FILE_OLD);
  return hasAuth && hasPam;
}

void ServiceInstaller::Install() {
  m_Logger("Copying binary module...");
  auto nativeExe = ResourceHelper::GetResource(":/res/natives/{}", EXE_MODULE_FILE);
  auto exePath = EXE_MODULE_DIR / EXE_MODULE_FILE;
  auto result = Shell::WriteBytes(exePath, nativeExe);
  if(!result)
    throw std::runtime_error(I18n::Get("error_file_write", exePath.string()));
  result = Shell::RunCommand(fmt::format("chmod +x {0} && chmod u+s {0}", exePath.string())).exitCode == 0;
  if(!result)
    throw std::runtime_error(I18n::Get("error_exec_setuid", exePath.string()));

  m_Logger("Copying PAM module...");
  Shell::CreateDir(PAM_MODULE_DIR);
  auto pamModule = ResourceHelper::GetResource(":/res/natives/{}", PAM_MODULE_FILE);
  auto pamPath = PAM_MODULE_DIR / PAM_MODULE_FILE;
  result = Shell::WriteBytes(pamPath, pamModule);
  if(!result)
    throw std::runtime_error(I18n::Get("error_file_write", pamPath.string()));

  m_Logger("Copying SSH module...");
  auto askpassExe = ResourceHelper::GetResource(":/res/natives/{}", SSH_MODULE_FILE);
  auto askpassPath = EXE_MODULE_DIR / SSH_MODULE_FILE;
  result = Shell::WriteBytes(askpassPath, askpassExe);
  if(!result)
    throw std::runtime_error(I18n::Get("error_file_write", askpassPath.string()));
  result = Shell::RunCommand(fmt::format("chmod +x {0} && chmod u+s {0}", askpassPath.string())).exitCode == 0;
  if(!result)
    throw std::runtime_error(I18n::Get("error_exec_setuid", askpassPath.string()));

  // Migration
  auto oldPamPath = PAM_MODULE_DIR / PAM_MODULE_FILE_OLD;
  if(std::filesystem::exists(oldPamPath)) {
    result = Shell::RemoveFile(oldPamPath);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_remove", oldPamPath.string()));
  }
  m_PAMHelper.MigrateConfigEntry("sudo", PAM_CONFIG_ENTRY_OLD, PAM_CONFIG_ENTRY);
  m_PAMHelper.MigrateConfigEntry("authorization", PAM_CONFIG_ENTRY_OLD, PAM_CONFIG_ENTRY);

  // ToDo: Firewall echo "pass in proto tcp from any to any port 43295" | sudo pfctl -ef -
  m_Logger("Done.");
}

void ServiceInstaller::Uninstall(bool fullUninstall) {
  m_Logger("Removing binary module...");
  auto exePath = EXE_MODULE_DIR / EXE_MODULE_FILE;
  auto result = true;
  if(std::filesystem::exists(exePath)) {
    result = Shell::RemoveFile(exePath);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_remove", exePath.string()));
  }

  m_Logger("Removing PAM module...");
  for(const auto &moduleFile : {PAM_MODULE_FILE, PAM_MODULE_FILE_OLD}) {
    auto pamPath = PAM_MODULE_DIR / moduleFile;
    if(!std::filesystem::exists(pamPath))
      continue;
    result = Shell::RemoveFile(pamPath);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_remove", pamPath.string()));
  }

  m_Logger("Removing SSH module...");
  auto askpassPath = EXE_MODULE_DIR / SSH_MODULE_FILE;
  if(std::filesystem::exists(askpassPath)) {
    result = Shell::RemoveFile(askpassPath);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_remove", askpassPath.string()));
  }

  if(fullUninstall) {
    m_Logger("Removing PAM configuration...");
    for(const auto &entry : {PAM_CONFIG_ENTRY, PAM_CONFIG_ENTRY_OLD}) {
      m_PAMHelper.SetConfigEntry("sudo", entry, false);
      m_PAMHelper.SetConfigEntry("authorization", entry, false);
    }
    if(EnvHelper::IsSshEnabled()) {
      m_Logger("Removing SSH integration...");
      EnvHelper::SetSshEnabled(false);
    }
  }
  m_Logger("Done.");
}

bool ServiceInstaller::IsProgramInstalled(const std::string &pathName) {
  return false;
}
