#include "MainWindow.h"

#include "installer/AppInstaller.h"
#include "installer/StartupChecks.h"
#include "installer/service/ServiceInstaller.h"
#include "shell/Shell.h"
#include "storage/AppSettings.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/ResourceHelper.h"

#ifdef APPLE
#include "storage/MacAgentStorage.h"
#endif

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

bool MainWindow::NeedsAccessibility() {
#ifdef APPLE
  if(!ServiceInstaller::IsInstalled())
    return false;
  return !MacAgentStorage::HasAccessibility().value_or(false);
#else
  return false;
#endif
}

void MainWindow::RequestAccessibility() {
#ifdef APPLE
  MacAgentStorage::RequestAccessibility();
#endif
}

void MainWindow::PerformStartupChecks(QObject *window) {
  if(m_LoadingThread.joinable())
    m_LoadingThread.join();
  m_LoadingThread = std::thread([window]() {
    auto finish = [window](bool success, bool needsReinstall) {
      QMetaObject::invokeMethod(window, "onStartupChecksFinished", Q_ARG(QVariant, success), Q_ARG(QVariant, needsReinstall));
    };

    auto error = StartupChecks::Run();
    if(error.has_value()) {
      QMetaObject::invokeMethod(window, "showFatalErrorMessage", Q_ARG(QVariant, QString::fromUtf8(error.value())));
      finish(false, false);
      return;
    }

    Shell::SetElevatorLostHandler([window]() {
      QMetaObject::invokeMethod(window, "showFatalErrorMessage",
                                Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_elevator_lost", I18n::Get("product_name")))));
    });
    finish(true, StartupChecks::NeedsReinstall());
  });
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
    auto installer = AppInstaller(logCallback);
    try {
      if(ServiceInstaller::IsInstalled())
        installer.Uninstall();
      else
        installer.Install();
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
    try {
      AppInstaller(logCallback).Reinstall();
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
