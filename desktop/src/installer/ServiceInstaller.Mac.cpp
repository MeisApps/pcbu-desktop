#include "ServiceInstaller.h"

#include <QCoreApplication>

#include "EnvHelper.h"
#include "shell/Shell.h"
#include "storage/AppSettings.h"
#include "utils/ResourceHelper.h"
#include "utils/StringUtils.h"

#define EXE_MODULE_DIR std::filesystem::path("/usr/local/sbin/")
#define EXE_MODULE_FILE "pcbu_auth"
#define SSH_MODULE_FILE "pcbu_ssh_askpass"

#define PAM_MODULE_DIR std::filesystem::path("/usr/local/lib/pam/")
#define PAM_MODULE_FILE "pam_pulseunlock.dylib"
#define PAM_MODULE_FILE_OLD "pam_pcbiounlock.dylib"

#define PAM_SUDO_LOCAL_CONFIG "sudo_local"
#define PAM_AUTHORIZATION_CONFIG "authorization"

#define PAM_CONFIG_ENTRY "auth sufficient /usr/local/lib/pam/pam_pulseunlock.dylib"
#define PAM_CONFIG_ENTRY_OLD "auth sufficient /usr/local/lib/pam/pam_pcbiounlock.dylib"

#define AGENT_BUNDLE_NAME "PulseUnlockAgent.app"
#define AGENT_BUNDLE_ID "com.meisapps.PulseUnlock.Agent"
#define AGENT_INSTALL_DIR std::filesystem::path("/Library/Application Support/PulseUnlock/")
#define AGENT_LABEL "com.meisapps.PulseUnlock.agent"
#define AGENT_PLIST_DIR std::filesystem::path("/Library/LaunchAgents/")
#define AGENT_PLIST_FILE "com.meisapps.PulseUnlock.agent.plist"

ServiceInstaller::ServiceInstaller(const std::function<void(const std::string &)> &logCallback) {
  m_Logger = logCallback;
  m_PAMHelper = PAMHelper(m_Logger);
}

std::vector<ServiceSetting> ServiceInstaller::GetSettings() {
  auto settings = AppSettings::Get();
  return {{"unlockBehavior",
           I18n::Get("service_setting_unlock_behavior"),
           {
               {"key_press_lock_only", I18n::Get("unlock_behavior_key_press_lock_only")},
               {"key_press", I18n::Get("unlock_behavior_key_press")},
               {"none", I18n::Get("unlock_behavior_none")},
           },
           settings.unlockBehavior,
           "key_press_lock_only"},
          {"sudo", I18n::Get("service_setting_sudo"), PAMHelper::HasConfigEntry(PAM_SUDO_LOCAL_CONFIG, PAM_CONFIG_ENTRY), true},
          {"macOS", I18n::Get("service_setting_mac_auth_dialog"), PAMHelper::HasConfigEntry(PAM_AUTHORIZATION_CONFIG, PAM_CONFIG_ENTRY), false},
          {"macLoginScreen", I18n::Get("service_setting_mac_login_screen"), settings.macLoginScreen, true},
          {"ssh", I18n::Get("service_setting_ssh"), EnvHelper::IsSshEnabled(), false}};
}

void ServiceInstaller::ApplySettings(const std::vector<ServiceSetting> &settings, bool useDefault) {
  for(auto setting : settings) {
    auto isEnabled = useDefault ? setting.defaultVal : setting.enabled;
    if(setting.id == "macLoginScreen") {
      auto storage = AppSettings::Get();
      storage.macLoginScreen = isEnabled;
      AppSettings::Save(storage);
    } else if(setting.id == "unlockBehavior") {
      auto storage = AppSettings::Get();
      storage.unlockBehavior = useDefault ? setting.defaultValue : setting.selectedValue;
      AppSettings::Save(storage);
    } else if(setting.id == "sudo") {
      m_PAMHelper.SetConfigEntry(PAM_SUDO_LOCAL_CONFIG, PAM_CONFIG_ENTRY, isEnabled);
    } else if(setting.id == "macOS") {
      m_PAMHelper.SetConfigEntry(PAM_AUTHORIZATION_CONFIG, PAM_CONFIG_ENTRY, isEnabled);
    } else if(setting.id == "ssh") {
      EnvHelper::SetSshEnabled(isEnabled);
    } else {
      spdlog::warn("Unknown service setting {}.", setting.id);
    }
  }
  m_PAMHelper.Commit();
}

bool ServiceInstaller::IsInstalled() {
  auto hasAuth = std::filesystem::exists(EXE_MODULE_DIR / EXE_MODULE_FILE);
  auto hasPam = std::filesystem::exists(PAM_MODULE_DIR / PAM_MODULE_FILE) || std::filesystem::exists(PAM_MODULE_DIR / PAM_MODULE_FILE_OLD);
  return hasAuth && hasPam;
}

void ServiceInstaller::Install() {
  m_Logger("Copying binary module...");
  Shell::CreateDir(EXE_MODULE_DIR);
  auto nativeExe = ResourceHelper::GetResource(":/res/natives/{}", EXE_MODULE_FILE);
  auto exePath = EXE_MODULE_DIR / EXE_MODULE_FILE;
  auto result = Shell::WriteBytes(exePath, nativeExe);
  if(!result)
    throw std::runtime_error(I18n::Get("error_file_write", exePath.string()));
  result = Shell::RunCommand(fmt::format("chown root:wheel {0} && chmod +x {0} && chmod u+s {0}", exePath.string())).exitCode == 0;
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
  result = Shell::RunCommand(fmt::format("chown root:wheel {0} && chmod +x {0} && chmod u+s {0}", askpassPath.string())).exitCode == 0;
  if(!result)
    throw std::runtime_error(I18n::Get("error_exec_setuid", askpassPath.string()));

  // Agent
  m_Logger("Installing lock screen agent...");
  auto agentSrc = std::filesystem::path(QCoreApplication::applicationDirPath().toStdString()) / ".." / "Library" / AGENT_BUNDLE_NAME;
  if(!std::filesystem::exists(agentSrc))
    throw std::runtime_error(I18n::Get("error_mac_agent_missing"));

  auto agentPath = AGENT_INSTALL_DIR / AGENT_BUNDLE_NAME;
  if(!Shell::Remove(agentPath, true))
    throw std::runtime_error(I18n::Get("error_file_remove", agentPath.string()));
  auto cmdResult =
      Shell::RunCommand(fmt::format(R"(mkdir -p {2} && ditto {0} {1} && chown -R root:wheel {1} && chmod -R go-w {1})",
                                    StringUtils::ShellQuote(agentSrc.string()), StringUtils::ShellQuote(agentPath.string()),
                                    StringUtils::ShellQuote(AGENT_INSTALL_DIR.string())));
  if(cmdResult.exitCode != 0)
    throw std::runtime_error(I18n::Get("error_file_write", agentPath.string()));
  Shell::RunCommand(
      fmt::format(R"(codesign -dv "{}" 2>&1 | grep -q "Signature=adhoc" && tccutil reset Accessibility {})", agentPath.string(), AGENT_BUNDLE_ID));

  auto plist = ResourceHelper::GetResource(":/res/mac/{}", AGENT_PLIST_FILE);
  auto plistPath = AGENT_PLIST_DIR / AGENT_PLIST_FILE;
  if(!Shell::WriteBytes(plistPath, plist))
    throw std::runtime_error(I18n::Get("error_file_write", plistPath.string()));
  Shell::RunCommand(fmt::format(R"(chown root:wheel "{0}" && chmod 644 "{0}")", plistPath.string()));

  cmdResult = Shell::RunCommand(fmt::format(R"(uid=$(stat -f %u /dev/console); if [ "$uid" != "0" ]; then )"
                                            R"(launchctl bootout "gui/$uid/{0}" >/dev/null 2>&1; )"
                                            R"(launchctl bootstrap "gui/$uid" "{1}" || (sleep 1 && launchctl bootstrap "gui/$uid" "{1}"); fi)",
                                            AGENT_LABEL, plistPath.string()));
  if(cmdResult.exitCode != 0)
    m_Logger(fmt::format("Failed starting lock screen agent: {}", cmdResult.output));

  // Migration
  m_PAMHelper.MigrateConfigEntry(PAM_AUTHORIZATION_CONFIG, PAM_CONFIG_ENTRY_OLD, PAM_CONFIG_ENTRY);
  m_PAMHelper.Commit();
  auto oldPamPath = PAM_MODULE_DIR / PAM_MODULE_FILE_OLD;
  if(std::filesystem::exists(oldPamPath)) {
    result = Shell::Remove(oldPamPath);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_remove", oldPamPath.string()));
  }

  // ToDo: Firewall echo "pass in proto tcp from any to any port 43295" | sudo pfctl -ef -
  m_Logger("Done.");
}

void ServiceInstaller::Uninstall(bool fullUninstall) {
  if(fullUninstall) {
    m_Logger("Removing PAM configuration...");
    for(const auto &entry : {PAM_CONFIG_ENTRY, PAM_CONFIG_ENTRY_OLD}) {
      m_PAMHelper.SetConfigEntry(PAM_SUDO_LOCAL_CONFIG, entry, false);
      m_PAMHelper.SetConfigEntry(PAM_AUTHORIZATION_CONFIG, entry, false);
    }
    m_PAMHelper.Commit();
  }

  m_Logger("Removing binary module...");
  auto exePath = EXE_MODULE_DIR / EXE_MODULE_FILE;
  auto result = true;
  if(std::filesystem::exists(exePath)) {
    result = Shell::Remove(exePath);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_remove", exePath.string()));
  }

  m_Logger("Removing PAM module...");
  for(const auto &moduleFile : {PAM_MODULE_FILE, PAM_MODULE_FILE_OLD}) {
    auto pamPath = PAM_MODULE_DIR / moduleFile;
    if(!std::filesystem::exists(pamPath))
      continue;
    result = Shell::Remove(pamPath);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_remove", pamPath.string()));
  }

  m_Logger("Removing SSH module...");
  auto askpassPath = EXE_MODULE_DIR / SSH_MODULE_FILE;
  if(std::filesystem::exists(askpassPath)) {
    result = Shell::Remove(askpassPath);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_remove", askpassPath.string()));
  }
  if(fullUninstall && EnvHelper::IsSshEnabled()) {
    m_Logger("Removing SSH integration...");
    EnvHelper::SetSshEnabled(false);
  }

  m_Logger("Removing lock screen agent...");
  Shell::RunCommand(fmt::format(
      R"(uid=$(stat -f %u /dev/console); if [ "$uid" != "0" ]; then launchctl bootout "gui/$uid/{}" >/dev/null 2>&1; fi; true)", AGENT_LABEL));
  auto plistPath = AGENT_PLIST_DIR / AGENT_PLIST_FILE;
  if(std::filesystem::exists(plistPath)) {
    result = Shell::Remove(plistPath);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_remove", plistPath.string()));
  }
  auto agentPath = AGENT_INSTALL_DIR / AGENT_BUNDLE_NAME;
  if(std::filesystem::exists(agentPath)) {
    result = Shell::Remove(agentPath, true);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_remove", agentPath.string()));
  }
  if(fullUninstall) {
    Shell::Remove(AGENT_INSTALL_DIR);
    Shell::RunCommand(fmt::format("tccutil reset Accessibility {}", AGENT_BUNDLE_ID));
  }

  m_Logger("Done.");
}

bool ServiceInstaller::IsProgramInstalled(const std::string &pathName) {
  return false;
}
