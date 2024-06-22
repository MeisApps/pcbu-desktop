#include "MainWindow.h"

#include "installer/ServiceInstaller.h"
#include "platform/PlatformHelper.h"
#include "shell/Shell.h"
#include "storage/AppSettings.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/AppInfo.h"
#include "utils/ResourceHelper.h"

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
    } catch(...) {}
    return {};
}

bool MainWindow::PerformStartupChecks(QObject *viewLoader, QObject *window) {
    if(!Shell::IsRunningAsAdmin()) {
        QMetaObject::invokeMethod(window, "showFatalErrorMessage", Q_ARG(QVariant, "Please run the app as admin/root."));
        return false;
    }
#if defined(LINUX) || defined(APPLE)
    if(Shell::RunUserCommand("which bash").exitCode != 0) {
        QMetaObject::invokeMethod(window, "showFatalErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_unix_missing_dep", "bash"))));
        return false;
    }
#ifdef LINUX
    if(Shell::RunUserCommand("test -f /etc/shadow").exitCode != 0) {
        QMetaObject::invokeMethod(window, "showFatalErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_unix_missing_dep", "Shadow file"))));
        return false;
    }
    /*if(!PlatformHelper::HasNativeLibrary("libcrypt.so.1")) {
        QMetaObject::invokeMethod(window, "showFatalErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_unix_missing_dep", "libcrypt.so.1 (libxcrypt-compat)"))));
        return false;
    }*/
    if(!PlatformHelper::HasNativeLibrary("libcrypto.so.3") || !PlatformHelper::HasNativeLibrary("libssl.so.3")) {
        QMetaObject::invokeMethod(window, "showFatalErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_unix_missing_dep", "OpenSSL 3"))));
        return false;
    }
    if(!PlatformHelper::HasNativeLibrary("libbluetooth.so") && !PlatformHelper::HasNativeLibrary("libbluetooth.so.3")) {
        QMetaObject::invokeMethod(window, "showFatalErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_unix_missing_dep", "libbluetooth"))));
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
        auto logCallback = [window](const std::string& str){
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
        } catch(const std::exception& ex) {
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
        auto logCallback = [window](const std::string& str){
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
        } catch(const std::exception& ex) {
            logCallback(ex.what());
            QMetaObject::invokeMethod(window, "finishLoadingScreen", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error"))));
        }
    });
}

void MainWindow::OnTestUnlockClicked(QObject *viewLoader, const QString &pairingId) {
    auto device = PairedDevicesStorage::GetDeviceByID(pairingId.toStdString());
    if(!device.has_value())
        return;
    auto result = Shell::RunUserCommand(fmt::format("/usr/local/sbin/pcbu_auth {} {}", device->userName, "PCBioUnlock Test")); // ToDo
    spdlog::info("Test result: {} (Code={})", result.output, result.exitCode);
}

void MainWindow::OnRemoveDeviceClicked(QObject *viewLoader, const QString& pairingId) {
    PairedDevicesStorage::RemoveDevice(pairingId.toStdString());
    Show(viewLoader);
}
