#include "Updater.h"

#include <unistd.h>

#include "shell/LocalShell.h"
#include "shell/Shell.h"
#include "utils/StringUtils.h"

std::string Updater::GetUpdateFileName() {
  return "Update-PulseUnlock.AppImage";
}

std::string Updater::InstallUpdate(const boost::filesystem::path &downloadPath) {
  auto quote = [](const boost::filesystem::path &path) { return StringUtils::ShellQuote(path.string()); };
  auto appImagePath = std::getenv("APPIMAGE");
  if(!appImagePath) {
    LocalShell::Remove(downloadPath.parent_path().string(), true);
    throw std::runtime_error("AppImage path not found.");
  }

  auto targetPath = boost::filesystem::path(appImagePath);
  auto stagingPath = targetPath.parent_path() / fmt::format(".pcbu-update-{}.AppImage", StringUtils::RandomString(16, false));
  auto replaceCmd = fmt::format("cp {0} {1} && chown --reference={2} {1} && chmod --reference={2} {1} && chmod +x {1} && mv -f {1} {2} "
                                "|| {{ rm -f {1}; exit 1; }}",
                                quote(downloadPath), quote(stagingPath), quote(targetPath));
  auto replaceResult = Shell::RunCommand(replaceCmd);
  LocalShell::Remove(downloadPath.parent_path().string(), true);
  if(replaceResult.exitCode != 0)
    throw std::runtime_error(
        fmt::format("Failed replacing AppImage. (Code={}, Output={})", replaceResult.exitCode, StringUtils::Trim(replaceResult.output)));
  return fmt::format("while ps -p {0} > /dev/null 2>&1; do sleep 1; done; {1}", getpid(), quote(targetPath));
}
