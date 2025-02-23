#include "ServiceInstaller.h"

#include "shell/Shell.h"
#include "storage/AppSettings.h"
#include "utils/ResourceHelper.h"

#define EXE_MODULE_DIR std::filesystem::path("/usr/local/sbin/")
#define EXE_MODULE_FILE "pcbu_auth"

#define PAM_MODULE_DIRS std::vector<std::filesystem::path>{"/lib/security/", "/lib64/security/"}
#define PAM_MODULE_FILE "pam_pcbiounlock.so"

#define PAM_CONFIG_ENTRY "auth sufficient pam_pcbiounlock.so"
#define PAM_CONFIG_ENTRY_SDDM                                                                                                                        \
  "auth [success=1 new_authtok_reqd=1 default=ignore] pam_unix.so try_first_pass likeauth nullok\nauth sufficient pam_pcbiounlock.so"

#define UFW_NAME "ufw"
#define FIREWALLD_NAME "firewall-cmd"
#define SELINUX_NAME "semodule"
#define SUDO_NAME "sudo"
#define POLKIT_NAME "pkexec"
#define GDM_NAME "gdm"
#define SDDM_NAME "sddm"
#define KDE_NAME "kde-open"
#define LIGHTDM_NAME "lightdm"
#define CINNAMON_NAME "cinnamon"

ServiceInstaller::ServiceInstaller(const std::function<void(const std::string &)> &logCallback) {
  m_Logger = logCallback;
  m_PAMHelper = PAMHelper(m_Logger);
}

std::vector<ServiceSetting> ServiceInstaller::GetSettings() {
  auto isLoginEnabled = PAMHelper::HasConfigEntry("gdm-password", PAM_CONFIG_ENTRY) || PAMHelper::HasConfigEntry("sddm", PAM_CONFIG_ENTRY_SDDM) ||
                        PAMHelper::HasConfigEntry("kde", PAM_CONFIG_ENTRY) || PAMHelper::HasConfigEntry("lightdm", PAM_CONFIG_ENTRY) ||
                        PAMHelper::HasConfigEntry("cinnamon-screensaver", PAM_CONFIG_ENTRY);
  auto hasLoginManager = IsProgramInstalled(GDM_NAME) || IsProgramInstalled(SDDM_NAME) || IsProgramInstalled(KDE_NAME) ||
                         IsProgramInstalled(LIGHTDM_NAME) || IsProgramInstalled(CINNAMON_NAME);
  return {{"sudo", I18n::Get("service_setting_sudo"), PAMHelper::HasConfigEntry("sudo", PAM_CONFIG_ENTRY), IsProgramInstalled(SUDO_NAME)},
          {"polkit", I18n::Get("service_setting_polkit"), PAMHelper::HasConfigEntry("polkit-1", PAM_CONFIG_ENTRY), IsProgramInstalled(POLKIT_NAME)},
          {"login", I18n::Get("service_setting_login_manager"), isLoginEnabled, hasLoginManager}};
}

void ServiceInstaller::ApplySettings(const std::vector<ServiceSetting> &settings, bool useDefault) {
  for(auto setting : settings) {
    auto isEnabled = useDefault ? setting.defaultVal : setting.enabled;
    if(setting.id == "sudo") {
      m_PAMHelper.SetConfigEntry("sudo", PAM_CONFIG_ENTRY, isEnabled);
    } else if(setting.id == "polkit") {
      m_PAMHelper.SetConfigEntry("polkit-1", PAM_CONFIG_ENTRY, isEnabled);
    } else if(setting.id == "login") {
      if(IsProgramInstalled(GDM_NAME))
        m_PAMHelper.SetConfigEntry("gdm-password", PAM_CONFIG_ENTRY, isEnabled);
      if(IsProgramInstalled(SDDM_NAME))
        m_PAMHelper.SetConfigEntry("sddm", PAM_CONFIG_ENTRY_SDDM, isEnabled);
      if(IsProgramInstalled(KDE_NAME))
        m_PAMHelper.SetConfigEntry("kde", PAM_CONFIG_ENTRY, isEnabled);
      if(IsProgramInstalled(LIGHTDM_NAME))
        m_PAMHelper.SetConfigEntry("lightdm", PAM_CONFIG_ENTRY, isEnabled);
      if(IsProgramInstalled(CINNAMON_NAME))
        m_PAMHelper.SetConfigEntry("cinnamon-screensaver", PAM_CONFIG_ENTRY, isEnabled);
    } else {
      spdlog::warn("Unknown service setting {}.", setting.id);
    }
  }
}

bool ServiceInstaller::IsInstalled() {
  for(const auto &pamDir : PAM_MODULE_DIRS) {
    if(std::filesystem::exists(pamDir / PAM_MODULE_FILE))
      return std::filesystem::exists(EXE_MODULE_DIR / EXE_MODULE_FILE);
  }
  return false;
}

void ServiceInstaller::Install() {
  m_Logger("Copying binary module...");
  auto nativeExe = ResourceHelper::GetResource(":/res/natives/{}", EXE_MODULE_FILE);
  auto exePath = EXE_MODULE_DIR / EXE_MODULE_FILE;
  auto result = Shell::WriteBytes(EXE_MODULE_DIR / EXE_MODULE_FILE, nativeExe);
  if(!result)
    throw std::runtime_error(I18n::Get("error_file_write", exePath.string()));
  result = Shell::RunCommand(fmt::format("chmod +x {0} && chmod u+s {0}", exePath.string())).exitCode == 0;
  if(!result)
    throw std::runtime_error(I18n::Get("error_exec_setuid", exePath.string()));

  m_Logger("Copying PAM module...");
  Shell::CreateDir("/lib/security");
  auto pamModule = ResourceHelper::GetResource(":/res/natives/{}", PAM_MODULE_FILE);
  for(const auto &pamDir : PAM_MODULE_DIRS) {
    if(!std::filesystem::exists(pamDir))
      continue;
    auto pamPath = pamDir / PAM_MODULE_FILE;
    result = Shell::WriteBytes(pamPath, pamModule);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_write", pamPath.string()));
  }

  auto settings = AppSettings::Get();
  if(IsProgramInstalled(UFW_NAME)) {
    m_Logger("Adding firewall rules (ufw)...");
    result = Shell::RunCommand(fmt::format("ufw allow {}/tcp", settings.pairingServerPort)).exitCode == 0;
    if(!result)
      m_Logger(I18n::Get("warning_firewall_rule_add", "ufw"));
  }
  if(IsProgramInstalled(FIREWALLD_NAME)) {
    m_Logger("Adding firewall rules (firewalld)...");
    result = Shell::RunCommand(fmt::format("firewall-cmd --zone=public --add-port={}/tcp --permanent", settings.pairingServerPort)).exitCode == 0 &&
             Shell::RunCommand("firewall-cmd --reload").exitCode == 0;
    if(!result)
      m_Logger(I18n::Get("warning_firewall_rule_add", "firewalld"));
  }
  if(IsProgramInstalled(SELINUX_NAME)) {
    m_Logger("Installing SELinux policy...");
    auto policyData = ResourceHelper::GetResource(":/res/selinux/pcbu_auth_policy.pp");
    auto tmpPath = std::filesystem::path("/tmp/pcbu_auth_policy.pp");
    Shell::WriteBytes(tmpPath, policyData);
    result = Shell::RunCommand(fmt::format("semodule -i \"{}\"", tmpPath.string())).exitCode == 0;
    if(!result)
      throw std::runtime_error(I18n::Get("error_selinux_policy_install"));
    Shell::RemoveFile(tmpPath);
  }
  m_Logger("Done.");
}

void ServiceInstaller::Uninstall() {
  m_Logger("Removing binary module...");
  auto exePath = EXE_MODULE_DIR / EXE_MODULE_FILE;
  auto result = Shell::RemoveFile(exePath);
  if(!result)
    throw std::runtime_error(I18n::Get("error_file_remove", exePath.string()));

  m_Logger("Removing PAM module...");
  for(const auto &pamDir : PAM_MODULE_DIRS) {
    auto pamPath = pamDir / PAM_MODULE_FILE;
    if(!std::filesystem::exists(pamPath))
      continue;
    result = Shell::RemoveFile(pamPath);
    if(!result)
      throw std::runtime_error(I18n::Get("error_file_remove", pamPath.string()));
  }

  auto settings = AppSettings::Get();
  if(IsProgramInstalled(UFW_NAME)) {
    m_Logger("Removing firewall rules (ufw)...");
    result = Shell::RunCommand(fmt::format("ufw delete allow {}/tcp", settings.pairingServerPort)).exitCode == 0;
    if(!result)
      m_Logger(I18n::Get("warning_firewall_rule_remove", "ufw"));
  }
  if(IsProgramInstalled(FIREWALLD_NAME)) {
    m_Logger("Removing firewall rules (firewalld)...");
    result =
        Shell::RunCommand(fmt::format("firewall-cmd --zone=public --remove-port={}/tcp --permanent", settings.pairingServerPort)).exitCode == 0 &&
        Shell::RunCommand("firewall-cmd --reload").exitCode == 0;
    if(!result)
      m_Logger(I18n::Get("warning_firewall_rule_remove", "firewalld"));
  }
  if(IsProgramInstalled(SELINUX_NAME)) {
    m_Logger("Removing SELinux policy...");
    result = Shell::RunCommand("semodule -r pcbu_auth_policy").exitCode == 0;
    if(!result)
      throw std::runtime_error(I18n::Get("error_selinux_policy_uninstall"));
  }
  m_Logger("Done.");
}

bool ServiceInstaller::IsProgramInstalled(const std::string &pathName) {
  return Shell::RunUserCommand(fmt::format("which {}", pathName)).exitCode == 0 ||
         Shell::RunUserCommand(fmt::format("systemctl cat {}", pathName)).exitCode == 0;
}
