#include "UpdaterWindow.h"

#include <regex>

#include <spdlog/spdlog.h>

#include "connection/web/HttpClient.h"
#include "platform/PlatformHelper.h"
#include "shell/LocalShell.h"
#include "shell/Shell.h"
#include "utils/AppInfo.h"
#include "utils/RestClient.h"
#include "utils/StringUtils.h"

#ifdef APPLE
#include <mach-o/dyld.h>
#include <unistd.h>
#elif defined(LINUX)
#include <unistd.h> // getpid()
#endif

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
      auto latestVersion = RestClient::CheckForUpdates("PulseUnlock", "Desktop");
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
    std::vector<uint8_t> fileData{};
    HttpClient::DownloadHandler contentCallback = [&](const uint8_t *data, size_t length) {
      fileData.insert(fileData.end(), data, data + length);
      return true;
    };
    HttpClient::ProgressHandler progressCallback = [window](uint64_t len, uint64_t total) {
      auto lenMib = (float)len / 1048576.F;
      auto percent = total > 0 ? (int)(len * 100 / total) : 0;
      auto progressText =
          total > 0 ? fmt::format("{}% ({:.2f}MiB / {:.2f}MiB)", percent, lenMib, (float)total / 1048576.F) : fmt::format("{:.2f}MiB", lenMib);
      QMetaObject::invokeMethod(window, "updateDownloadProgress", Q_ARG(QVariant, percent), Q_ARG(QVariant, QString::fromUtf8(progressText)));
      return true;
    };

    auto options = HttpOptions();
    options.connectTimeout = std::chrono::seconds(5);
    options.transferTimeout = std::chrono::seconds(30);
    options.bodyLimit = 0;
    options.maxRedirects = 5;

    auto client = HttpClient();
    auto result = client.Download("https://meis-apps.com" + GetDownloadURL(), contentCallback, progressCallback, options);
    if(!result.ok || result.status != 200 || fileData.empty()) {
      auto errText = "Error downloading update.";
      spdlog::error("{} (Status={}, Error={})", errText, result.status, result.error);
      QMetaObject::invokeMethod(window, "closeUpdaterWindow", Q_ARG(QVariant, QString::fromUtf8(errText)));
      return;
    }
    auto contentHeader = result.GetHeader("Content-Disposition");
#ifdef WINDOWS
    std::string downloadFileName = "Update-PulseUnlock.exe";
#elif LINUX
    std::string downloadFileName = "Update-PulseUnlock.AppImage";
#elif APPLE
    std::string downloadFileName = "Update-PulseUnlock.dmg";
#endif
    std::regex regex(R"(.*filename=\"(.*)\")");
    std::smatch matches{};
    if(std::regex_search(contentHeader, matches, regex))
      if(matches.size() >= 2)
        downloadFileName = "Update-" + matches[1].str();

    auto dlPath = boost::filesystem::path(PlatformHelper::GetTempDir().native()) / downloadFileName;
    spdlog::info("Saving update to '{}'...", dlPath.string());
    if(!LocalShell::WriteBytes(dlPath.string(), fileData)) {
      auto errText = "Error saving update.";
      spdlog::error(errText);
      QMetaObject::invokeMethod(window, "closeUpdaterWindow", Q_ARG(QVariant, QString::fromUtf8(errText)));
      return;
    }
    auto updateCmd = dlPath.string();
#ifndef WINDOWS
    try {
      updateCmd = InstallUpdate(dlPath);
    } catch(const std::exception &ex) {
      auto errText = "Error installing update.";
      spdlog::error("{} ({})", errText, ex.what());
      QMetaObject::invokeMethod(window, "closeUpdaterWindow", Q_ARG(QVariant, QString::fromUtf8(errText)));
      return;
    }
#endif
    spdlog::info("Starting update...");
    LocalShell::SpawnCommand(updateCmd);
    QMetaObject::invokeMethod(QCoreApplication::instance(), []() { QCoreApplication::exit(0); }, Qt::QueuedConnection);
  });
}

#ifdef LINUX
std::string UpdaterWindow::InstallUpdate(const boost::filesystem::path &downloadPath) {
  auto quote = [](const boost::filesystem::path &path) { return StringUtils::ShellQuote(path.string()); };
  auto appImagePath = std::getenv("APPIMAGE");
  if(!appImagePath) {
    LocalShell::Remove(downloadPath.string());
    throw std::runtime_error("AppImage path not found.");
  }

  auto targetPath = boost::filesystem::path(appImagePath);
  auto stagingPath = targetPath.parent_path() / fmt::format(".pcbu-update-{}.AppImage", StringUtils::RandomString(16, false));
  auto replaceCmd = fmt::format("cp {0} {1} && chown --reference={2} {1} && chmod --reference={2} {1} && chmod +x {1} && mv -f {1} {2} "
                                "|| {{ rm -f {1}; exit 1; }}",
                                quote(downloadPath), quote(stagingPath), quote(targetPath));
  auto replaceResult = Shell::RunCommand(replaceCmd);
  LocalShell::Remove(downloadPath.string());
  if(replaceResult.exitCode != 0)
    throw std::runtime_error(fmt::format("Failed replacing AppImage. (Code={}, Output={})", replaceResult.exitCode, StringUtils::Trim(replaceResult.output)));
  return fmt::format("while ps -p {0} > /dev/null 2>&1; do sleep 1; done; {1}", getpid(), quote(targetPath));
}
#elif APPLE
boost::filesystem::path UpdaterWindow::GetAppBundlePath() {
  char exePath[PATH_MAX];
  uint32_t pathSize = sizeof(exePath);
  if(_NSGetExecutablePath(exePath, &pathSize) != 0)
    return {};
  auto resolvedPath = boost::filesystem::canonical(exePath);
  boost::filesystem::path appBundlePath{};
  for(auto it = resolvedPath.begin(); it != resolvedPath.end(); ++it) {
    if(it->string().find(".app") != std::string::npos) {
      appBundlePath = resolvedPath.root_path();
      for(auto jt = resolvedPath.begin(); jt != std::next(it); ++jt)
        appBundlePath /= *jt;
      break;
    }
  }
  return appBundlePath;
}

std::string UpdaterWindow::InstallUpdate(const boost::filesystem::path &downloadPath) {
  auto quote = [](const boost::filesystem::path &path) { return StringUtils::ShellQuote(path.string()); };
  auto bundlePath = GetAppBundlePath();
  if(bundlePath.empty()) {
    LocalShell::Remove(downloadPath.string());
    throw std::runtime_error("App bundle not found.");
  }

  auto id = StringUtils::RandomString(16, false);
  auto mountDir = boost::filesystem::path(PlatformHelper::GetTempDir().native()) / fmt::format("pcbu-update-{}", id);
  auto mountCmd = LocalShell::RunUserCommand(fmt::format("hdiutil attach -nobrowse -readonly -noautoopen -mountpoint {} {}", quote(mountDir), quote(downloadPath)));
  if(mountCmd.exitCode != 0) {
    LocalShell::Remove(downloadPath.string());
    throw std::runtime_error(fmt::format("Failed mounting update. (Output={})", StringUtils::Trim(mountCmd.output)));
  }
  auto cleanup = [&]() {
    LocalShell::RunUserCommand(fmt::format("hdiutil detach -force {}", quote(mountDir)));
    LocalShell::Remove(downloadPath.string());
  };

  boost::filesystem::path newAppPath{};
  boost::system::error_code ec{};
  for(const auto &entry : boost::filesystem::directory_iterator(mountDir, ec)) {
    if(entry.path().extension() == ".app") {
      newAppPath = entry.path();
      break;
    }
  }
  if(newAppPath.empty()) {
    cleanup();
    throw std::runtime_error("No app found in update.");
  }

  auto parentDir = bundlePath.parent_path();
  auto stagingPath = parentDir / fmt::format(".pcbu-update-{}.app", id);
  auto oldPath = parentDir / fmt::format(".pcbu-old-{}.app", id);
  auto replaceCmd = fmt::format("ditto {0} {1} && chown -R \"$(stat -f %u:%g {2})\" {1} && mv {2} {3} && {{ mv {1} {2} || {{ mv {3} {2}; false; }}; }} "
                                "|| {{ rm -rf {1}; exit 1; }}; rm -rf {3}; true",
                                quote(newAppPath), quote(stagingPath), quote(bundlePath), quote(oldPath));
  auto replaceResult = Shell::RunCommand(replaceCmd);
  cleanup();
  if(replaceResult.exitCode != 0)
    throw std::runtime_error(fmt::format("Failed replacing app. (Code={}, Output={})", replaceResult.exitCode, StringUtils::Trim(replaceResult.output)));
  return fmt::format("while ps -p {0} > /dev/null 2>&1; do sleep 1; done; open {1}", getpid(), quote(bundlePath));
}
#endif

std::string UpdaterWindow::GetDownloadURL() {
  auto os = AppInfo::GetOperatingSystem();
  auto arch = AppInfo::GetArchitecture();
  if(os == "Windows")
    os = "win";
  if(os == "Linux")
    os = "linux";
  if(os == "macOS")
    os = "mac";
  return fmt::format("/download?product=PCBioUnlock&platform={}_{}", os, arch);
}
