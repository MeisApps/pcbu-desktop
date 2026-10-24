#include "AppInstaller.h"

#include "installer/service/ServiceInstaller.h"
#include "storage/AppSettings.h"
#include "storage/LoggingSystem.h"

AppInstaller::AppInstaller(const std::function<void(const std::string &)> &logCallback) {
  m_Logger = logCallback;
}

void AppInstaller::Install() {
  Migrate();
  auto installer = ServiceInstaller(m_Logger);
  installer.Install();
  installer.ApplySettings(installer.GetSettings(), true);
  AppSettings::SetInstalledVersion(ServiceInstaller::IsInstalled());
}

void AppInstaller::Uninstall() {
  Migrate();
  auto installer = ServiceInstaller(m_Logger);
  installer.Uninstall(true);
  AppSettings::SetInstalledVersion(ServiceInstaller::IsInstalled());
}

void AppInstaller::Reinstall() {
  auto installer = ServiceInstaller(m_Logger);
  if(ServiceInstaller::IsInstalled())
    installer.Uninstall(false);
  installer.Install();
  installer.ApplySettings(installer.GetSettings(), false);
  Migrate();
  AppSettings::SetInstalledVersion(true);
}

void AppInstaller::Migrate() {
  if(!AppSettings::NeedsMigration())
    return;

  m_Logger("Migrating data directory...");
  LoggingSystem::SetWriteToFile(false);
  try {
    AppSettings::MigrateBaseDir();
  } catch(const std::exception &ex) {
    LoggingSystem::SetWriteToFile(true);
    AppSettings::SetInstalledVersion(false);
    m_Logger(fmt::format("Migration failed: {}", ex.what()));
  }
  LoggingSystem::SetWriteToFile(true);
  m_Logger("Done.");
}
