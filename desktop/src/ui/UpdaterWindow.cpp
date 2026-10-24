#include "UpdaterWindow.h"

#include <QCoreApplication>
#include <spdlog/spdlog.h>

#include "shell/LocalShell.h"
#include "updater/Updater.h"
#include "utils/AppInfo.h"

UpdaterWindow::~UpdaterWindow() {
  if(m_CheckThread.joinable())
    m_CheckThread.join();
  if(m_DownloadThread.joinable())
    m_DownloadThread.join();
}

QString UpdaterWindow::GetAppVersion() {
  return QString::fromUtf8(AppInfo::GetVersion());
}

QString UpdaterWindow::GetLatestVersion() {
  std::lock_guard<std::mutex> lock(m_VersionMutex);
  return m_LatestVersion;
}

void UpdaterWindow::CheckForUpdates(QObject *window) {
  m_CheckThread = std::thread([this, window]() {
    try {
      auto latestVersion = Updater::GetLatestVersion();
      m_VersionMutex.lock();
      m_LatestVersion = QString::fromUtf8(latestVersion);
      m_VersionMutex.unlock();
      spdlog::info("Latest version: {}", latestVersion);
      if(AppInfo::CompareVersion(AppInfo::GetVersion(), latestVersion) == 1)
        QMetaObject::invokeMethod(window, "showUpdaterWindow");
    } catch(const std::exception &ex) {
      spdlog::error("Failed checking for updates: {}", ex.what());
      m_VersionMutex.lock();
      m_LatestVersion = {};
      m_VersionMutex.unlock();
    }
  });
}

void UpdaterWindow::OnDownloadClicked(QObject *window) {
  m_DownloadThread = std::thread([window]() {
    HttpClient::ProgressHandler progressCallback = [window](uint64_t len, uint64_t total) {
      auto lenMib = (float)len / 1048576.F;
      auto percent = total > 0 ? (int)(len * 100 / total) : 0;
      auto progressText =
          total > 0 ? fmt::format("{}% ({:.2f}MiB / {:.2f}MiB)", percent, lenMib, (float)total / 1048576.F) : fmt::format("{:.2f}MiB", lenMib);
      QMetaObject::invokeMethod(window, "updateDownloadProgress", Q_ARG(QVariant, percent), Q_ARG(QVariant, QString::fromUtf8(progressText)));
      return true;
    };

    std::string updateCmd{};
    try {
      updateCmd = Updater::DownloadUpdate(progressCallback);
    } catch(const std::exception &ex) {
      QMetaObject::invokeMethod(window, "closeUpdaterWindow", Q_ARG(QVariant, QString::fromUtf8(ex.what())));
      return;
    }
    spdlog::info("Starting update...");
    LocalShell::SpawnCommand(updateCmd);
    QMetaObject::invokeMethod(QCoreApplication::instance(), []() { QCoreApplication::exit(0); }, Qt::QueuedConnection);
  });
}
