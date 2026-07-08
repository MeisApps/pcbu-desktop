#include "MainWindow.h"

#include "installer/ServiceInstaller.h"
#include "platform/PlatformHelper.h"
#include "shell/Shell.h"
#include "storage/AppSettings.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/AppInfo.h"
#include "utils/ResourceHelper.h"

#include <algorithm>
#include <regex>

MainWindow::~MainWindow() {
  if(m_LoadingThread.joinable())
    m_LoadingThread.join();
}

bool MainWindow::IsInstalled() {
  return ServiceInstaller::IsInstalled();
}

bool MainWindow::IsPaired() {
  return !PairedDevicesStorage::GetDevices().empty();
}

QString MainWindow::GetInstalledVersion() {
  auto installedVersion = AppSettings::Get().installedVersion;
  if(installedVersion.empty())
    return QString::fromUtf8(I18n::Get("installed_version_none"));
  return QString::fromUtf8(installedVersion);
}

QString MainWindow::GetLicenseText() {
  try {
    return QString::fromUtf8(ResourceHelper::GetResource(":/res/licenses.txt"));
  } catch(...) {
  }
  return {};
}

QVariantMap MainWindow::GetDeviceTcpAddresses(const QString &pairingId) {
  QVariantMap result{};
  auto device = PairedDevicesStorage::GetDeviceByID(pairingId.toStdString());
  if(!device.has_value())
    return result;

  result["primaryIpAddress"] = QString::fromUtf8(device->ipAddress);
  result["secondaryIpAddress"] = QString::fromUtf8(device->secondaryIpAddress);
  result["lastSuccessfulIpAddress"] = QString::fromUtf8(device->lastSuccessfulIpAddress);
  return result;
}

bool MainWindow::SetDeviceTcpAddresses(const QString &pairingId, const QString &primaryIpAddress, const QString &secondaryIpAddress) {
  static const std::regex ipV4Regex(R"(^(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)(?:\.(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)){3}$)");

  auto primary = primaryIpAddress.trimmed().toStdString();
  auto secondary = secondaryIpAddress.trimmed().toStdString();
  if(primary.empty() || !std::regex_match(primary, ipV4Regex))
    return false;
  if(!secondary.empty() && !std::regex_match(secondary, ipV4Regex))
    return false;

  auto devices = PairedDevicesStorage::GetDevices();
  auto deviceIt = std::ranges::find_if(devices, [&pairingId](const PairedDevice &device) { return device.id == pairingId.toStdString(); });
  if(deviceIt == devices.end() || deviceIt->pairingMethod != PairingMethod::TCP)
    return false;

  deviceIt->ipAddress = primary;
  deviceIt->secondaryIpAddress = secondary;
  if(deviceIt->lastSuccessfulIpAddress != primary && deviceIt->lastSuccessfulIpAddress != secondary)
    deviceIt->lastSuccessfulIpAddress = {};
  PairedDevicesStorage::SaveDevices(devices);
  return true;
}

bool MainWindow::PerformStartupChecks(QObject *viewLoader, QObject *window) {
  if(!Shell::IsRunningAsAdmin()) {
    QMetaObject::invokeMethod(window, "showFatalErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_not_admin"))));
    return false;
  }
#if defined(LINUX) || defined(APPLE)
  if(Shell::RunUserCommand("which bash").exitCode != 0) {
    QMetaObject::invokeMethod(window, "showFatalErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_unix_missing_dep", "bash"))));
    return false;
  }
#ifdef LINUX
  if(Shell::RunUserCommand("test -f /etc/shadow").exitCode != 0) {
    QMetaObject::invokeMethod(window, "showFatalErrorMessage",
                              Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_unix_missing_dep", "Shadow file"))));
    return false;
  }
  if(!PlatformHelper::HasNativeLibrary("libcrypt.so.1")) {
    QMetaObject::invokeMethod(window, "showFatalErrorMessage",
                              Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_unix_missing_dep", "libcrypt.so.1 (libxcrypt-compat)"))));
    return false;
  }
  if(!PlatformHelper::HasNativeLibrary("libcrypto.so.3") || !PlatformHelper::HasNativeLibrary("libssl.so.3")) {
    QMetaObject::invokeMethod(window, "showFatalErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_unix_missing_dep", "OpenSSL 3"))));
    return false;
  }
  if(!PlatformHelper::HasNativeLibrary("libbluetooth.so") && !PlatformHelper::HasNativeLibrary("libbluetooth.so.3")) {
    QMetaObject::invokeMethod(window, "showFatalErrorMessage",
                              Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_unix_missing_dep", "libbluetooth"))));
    return false;
  }
#endif
#endif
  const auto installedVersion = AppSettings::Get().installedVersion;
  if(ServiceInstaller::IsInstalled() && (AppInfo::CompareVersion(installedVersion, AppInfo::GetVersion()) == 1 || installedVersion.empty()))
    OnReinstallClicked(window);
  return true;
}

void MainWindow::Show(QObject *viewLoader) {
  QMetaObject::invokeMethod(viewLoader, "setSource", Q_ARG(QUrl, QUrl("qrc:/ui/forms/MainForm.qml")));
}

void MainWindow::OnInstallClicked(QObject *window) {
  if(m_LoadingThread.joinable())
    m_LoadingThread.join();
  m_LoadingThread = std::thread([window]() {
    QMetaObject::invokeMethod(window, "showLoadingScreen", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("please_wait"))));
    auto logCallback = [window](const std::string &str) {
      spdlog::info(str);
      QMetaObject::invokeMethod(window, "appendLoadingOutput", Q_ARG(QVariant, QString::fromUtf8(str)));
    };
    auto installer = ServiceInstaller(logCallback);
    try {
      if(ServiceInstaller::IsInstalled()) {
        installer.Uninstall();
        installer.ClearSettings();
      } else {
        installer.Install();
        installer.ApplySettings(installer.GetSettings(), true);
      }
      AppSettings::SetInstalledVersion(ServiceInstaller::IsInstalled());
      QMetaObject::invokeMethod(window, "finishLoadingScreen", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("success"))));
    } catch(const std::exception &ex) {
      logCallback(ex.what());
      QMetaObject::invokeMethod(window, "finishLoadingScreen", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error"))));
    }
  });
}

void MainWindow::OnReinstallClicked(QObject *window) {
  if(m_LoadingThread.joinable())
    m_LoadingThread.join();
  m_LoadingThread = std::thread([window]() {
    QMetaObject::invokeMethod(window, "showLoadingScreen", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("please_wait"))));
    auto logCallback = [window](const std::string &str) {
      spdlog::info(str);
      QMetaObject::invokeMethod(window, "appendLoadingOutput", Q_ARG(QVariant, QString::fromUtf8(str)));
    };
    auto installer = ServiceInstaller(logCallback);
    try {
      if(ServiceInstaller::IsInstalled())
        installer.Uninstall();
      installer.Install();
      installer.ApplySettings(installer.GetSettings(), false);
      AppSettings::SetInstalledVersion(true);
      QMetaObject::invokeMethod(window, "finishLoadingScreen", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("success"))));
    } catch(const std::exception &ex) {
      logCallback(ex.what());
      QMetaObject::invokeMethod(window, "finishLoadingScreen", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error"))));
    }
  });
}

void MainWindow::OnRemoveDeviceClicked(QObject *viewLoader, const QString &pairingId) {
  PairedDevicesStorage::RemoveDevice(pairingId.toStdString());
  Show(viewLoader);
}
