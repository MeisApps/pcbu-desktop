#include "Updater.h"

#include <spdlog/spdlog.h>

#include "shell/LocalShell.h"
#include "shell/Shell.h"
#include "utils/StringUtils.h"

void Updater::InstallCliUpdate(const boost::filesystem::path &targetPath, const HttpClient::ProgressHandler &progressCallback) {
  auto dlPath = Download("cli", "Update-PulseUnlock-CLI.tar.gz", progressCallback);
  auto quote = [](const boost::filesystem::path &path) { return StringUtils::ShellQuote(path.string()); };
  auto extractDir = dlPath.parent_path() / "extract";
  auto stagingPath = targetPath.parent_path() / fmt::format(".pulseunlock-update-{}", StringUtils::RandomString(16, false));
  auto replaceCmd = fmt::format("trap '' INT; mkdir -p {0} && tar -xzf {1} -C {0} && src=$(find {0} -type f -name pulseunlock | head -n 1) && [ -n \"$src\" ] "
                                "&& cp \"$src\" {2} && chmod 755 {2} && mv -f {2} {3}; status=$?; rm -rf {0} {2}; exit $status",
                                quote(extractDir), quote(dlPath), quote(stagingPath), quote(targetPath));
  auto replaceResult = Shell::RunCommand(replaceCmd);
  LocalShell::Remove(dlPath.parent_path().string(), true);
  if(replaceResult.exitCode != 0) {
    auto errText = "Error installing update.";
    spdlog::error("{} (Code={}, Output={})", errText, replaceResult.exitCode, StringUtils::Trim(replaceResult.output));
    throw std::runtime_error(errText);
  }
}
