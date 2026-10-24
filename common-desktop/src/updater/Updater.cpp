#include "Updater.h"

#include <regex>
#include <spdlog/spdlog.h>

#include "platform/PlatformHelper.h"
#include "shell/LocalShell.h"
#include "utils/AppInfo.h"
#include "utils/RestClient.h"
#include "utils/StringUtils.h"

std::string Updater::GetLatestVersion() {
  return RestClient::CheckForUpdates("PulseUnlock", "Desktop");
}

std::string Updater::DownloadUpdate(const HttpClient::ProgressHandler &progressCallback) {
  auto dlPath = Download({}, GetUpdateFileName(), progressCallback);
  try {
    return InstallUpdate(dlPath);
  } catch(const std::exception &ex) {
    auto errText = "Error installing update.";
    spdlog::error("{} ({})", errText, ex.what());
    throw std::runtime_error(errText);
  }
}

boost::filesystem::path Updater::Download(const std::string &variant, const std::string &defaultFileName,
                                          const HttpClient::ProgressHandler &progressCallback) {
  std::vector<uint8_t> fileData{};
  HttpClient::DownloadHandler contentCallback = [&](const uint8_t *data, size_t length) {
    fileData.insert(fileData.end(), data, data + length);
    return true;
  };

  auto options = HttpOptions();
  options.connectTimeout = std::chrono::seconds(5);
  options.transferTimeout = std::chrono::seconds(30);
  options.bodyLimit = 0;
  options.maxRedirects = 5;

  auto client = HttpClient();
  auto result = client.Download("https://meis-apps.com" + GetDownloadURL(variant), contentCallback, progressCallback, options);
  if(!result.ok || result.status != 200 || fileData.empty()) {
    auto errText = "Error downloading update.";
    spdlog::error("{} (Status={}, Error={})", errText, result.status, result.error);
    throw std::runtime_error(errText);
  }
  auto contentHeader = result.GetHeader("Content-Disposition");
  auto downloadFileName = defaultFileName;
  std::regex regex(R"(.*filename=\"(.*)\")");
  std::smatch matches{};
  if(std::regex_search(contentHeader, matches, regex))
    if(matches.size() >= 2)
      downloadFileName = "Update-" + boost::filesystem::path(matches[1].str()).filename().string();

  auto dlDir = CreateDownloadDir();
  auto dlPath = dlDir / downloadFileName;
  spdlog::info("Saving update to '{}'...", dlPath.string());
  if(!LocalShell::WriteBytes(dlPath.string(), fileData)) {
    LocalShell::Remove(dlDir.string(), true);
    auto errText = "Error saving update.";
    spdlog::error(errText);
    throw std::runtime_error(errText);
  }
  return dlPath;
}

boost::filesystem::path Updater::CreateDownloadDir() {
  auto dlDir = boost::filesystem::path(PlatformHelper::GetTempDir().native()) / fmt::format("pcbu-download-{}", StringUtils::RandomString(16, false));
  boost::system::error_code ec{};
  if(!boost::filesystem::create_directory(dlDir, ec)) {
    auto errText = "Error saving update.";
    spdlog::error("{} (Path={}, Error={})", errText, dlDir.string(), ec.message());
    throw std::runtime_error(errText);
  }
  return dlDir;
}

std::string Updater::GetDownloadURL(const std::string &variant) {
  auto os = AppInfo::GetOperatingSystem();
  auto arch = AppInfo::GetArchitecture();
  if(os == "Windows")
    os = "win";
  if(os == "Linux")
    os = "linux";
  if(os == "macOS")
    os = "mac";
  if(!variant.empty())
    os += "_" + variant;
  return fmt::format("/download?product=PCBioUnlock&platform={}_{}", os, arch);
}
