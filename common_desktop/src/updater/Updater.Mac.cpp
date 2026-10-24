#include "Updater.h"

#include <mach-o/dyld.h>
#include <unistd.h>

#include "platform/PlatformHelper.h"
#include "shell/LocalShell.h"
#include "shell/Shell.h"
#include "utils/StringUtils.h"

std::string Updater::GetUpdateFileName() {
  return "Update-PulseUnlock.dmg";
}

std::string Updater::InstallUpdate(const boost::filesystem::path &downloadPath) {
  auto quote = [](const boost::filesystem::path &path) { return StringUtils::ShellQuote(path.string()); };
  auto bundlePath = GetAppBundlePath();
  if(bundlePath.empty()) {
    LocalShell::Remove(downloadPath.parent_path().string(), true);
    throw std::runtime_error("App bundle not found.");
  }

  auto id = StringUtils::RandomString(16, false);
  auto mountDir = boost::filesystem::path(PlatformHelper::GetTempDir().native()) / fmt::format("pcbu-update-{}", id);
  auto mountCmd = LocalShell::RunUserCommand(
      fmt::format("hdiutil attach -nobrowse -readonly -noautoopen -mountpoint {} {}", quote(mountDir), quote(downloadPath)));
  if(mountCmd.exitCode != 0) {
    LocalShell::Remove(downloadPath.parent_path().string(), true);
    throw std::runtime_error(fmt::format("Failed mounting update. (Output={})", StringUtils::Trim(mountCmd.output)));
  }
  auto cleanup = [&]() {
    LocalShell::RunUserCommand(fmt::format("hdiutil detach -force {}", quote(mountDir)));
    LocalShell::Remove(downloadPath.parent_path().string(), true);
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
  auto replaceCmd =
      fmt::format("ditto {0} {1} && chown -R \"$(stat -f %u:%g {2})\" {1} && mv {2} {3} && {{ mv {1} {2} || {{ mv {3} {2}; false; }}; }} "
                  "|| {{ rm -rf {1}; exit 1; }}; rm -rf {3}; true",
                  quote(newAppPath), quote(stagingPath), quote(bundlePath), quote(oldPath));
  auto replaceResult = Shell::RunCommand(replaceCmd);
  cleanup();
  if(replaceResult.exitCode != 0)
    throw std::runtime_error(
        fmt::format("Failed replacing app. (Code={}, Output={})", replaceResult.exitCode, StringUtils::Trim(replaceResult.output)));
  return fmt::format("while ps -p {0} > /dev/null 2>&1; do sleep 1; done; open {1}", getpid(), quote(bundlePath));
}

boost::filesystem::path Updater::GetAppBundlePath() {
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
