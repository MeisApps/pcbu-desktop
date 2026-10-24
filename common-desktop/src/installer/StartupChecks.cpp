#include "StartupChecks.h"

#include "installer/service/ServiceInstaller.h"
#include "platform/PlatformHelper.h"
#include "shell/LocalShell.h"
#include "shell/Shell.h"
#include "storage/AppSettings.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/AppInfo.h"
#include "utils/I18n.h"

std::optional<std::string> StartupChecks::Run() {
  Shell::Init();
  auto testCmdResult = Shell::RunCommand("echo");
  if(!Shell::HasAdmin() || testCmdResult.exitCode != 0) {
#ifdef WINDOWS
    auto errorKey = "error_not_admin";
#elif LINUX
    auto errorKey = "error_not_root";
#elif APPLE
    auto errorKey = "error_mac_admin_required";
#endif
    return I18n::Get(errorKey, I18n::Get("product_name"));
  }

#if defined(LINUX) || defined(APPLE)
  if(LocalShell::RunUserCommand("which bash").exitCode != 0)
    return I18n::Get("error_unix_missing_dep", "bash");
#ifdef LINUX
  if(LocalShell::RunUserCommand("test -f /etc/shadow").exitCode != 0)
    return I18n::Get("error_unix_missing_dep", "Shadow file");
  if(!PlatformHelper::HasNativeLibrary("libcrypt.so.1"))
    return I18n::Get("error_unix_missing_dep", "libcrypt.so.1 (libxcrypt-compat)");
  if(!PlatformHelper::HasNativeLibrary("libcrypto.so.3") || !PlatformHelper::HasNativeLibrary("libssl.so.3"))
    return I18n::Get("error_unix_missing_dep", "OpenSSL 3");
#endif
#endif
  return {};
}

bool StartupChecks::NeedsReinstall() {
  AppSettings::InvalidateCache();
  PairedDevicesStorage::InvalidateCache();
  PairedDevicesStorage::GetDevices();
  auto installedVersion = AppSettings::Get().installedVersion;
  return ServiceInstaller::IsInstalled() && (AppInfo::CompareVersion(installedVersion, AppInfo::GetVersion()) == 1 || installedVersion.empty());
}
