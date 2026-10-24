#ifndef PCBU_DESKTOP_UPDATER_H
#define PCBU_DESKTOP_UPDATER_H

#include <boost/filesystem.hpp>
#include <string>

#include "connection/web/HttpClient.h"

class Updater {
public:
  static std::string GetLatestVersion();
  static std::string DownloadUpdate(const HttpClient::ProgressHandler &progressCallback);

#ifndef WINDOWS
  static void InstallCliUpdate(const boost::filesystem::path &targetPath, const HttpClient::ProgressHandler &progressCallback);
#endif

private:
  Updater() = default;

  static boost::filesystem::path Download(const std::string &variant, const std::string &defaultFileName,
                                          const HttpClient::ProgressHandler &progressCallback);
  static boost::filesystem::path CreateDownloadDir();

  static std::string GetDownloadURL(const std::string &variant);
  static std::string GetUpdateFileName();

  static std::string InstallUpdate(const boost::filesystem::path &downloadPath);

#ifdef APPLE
  static boost::filesystem::path GetAppBundlePath();
#endif
};

#endif // PCBU_DESKTOP_UPDATER_H
