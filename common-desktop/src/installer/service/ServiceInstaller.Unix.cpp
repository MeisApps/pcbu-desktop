#include "ServiceInstaller.h"

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/filesystem.hpp>

#include "shell/Shell.h"
#include "utils/StringUtils.h"

#ifdef APPLE
#define CLI_DIR boost::filesystem::path("/usr/local/bin/")
#else
#define CLI_DIR boost::filesystem::path("/usr/bin/")
#endif
#define CLI_FILE "pulseunlock"

void ServiceInstaller::InstallCli() {
  auto cliSrc = boost::dll::program_location().parent_path() / CLI_FILE;
  auto cliPath = CLI_DIR / CLI_FILE;
  if(!boost::filesystem::exists(cliSrc)) {
    m_Logger("CLI not found, skipping.");
    return;
  }
  boost::system::error_code ec{};
  if(boost::filesystem::equivalent(cliSrc, cliPath, ec))
    return;

  m_Logger("Copying CLI...");
  auto stagingPath = CLI_DIR / fmt::format(".pulseunlock-{}", StringUtils::RandomString(16, false));
  auto cmd = fmt::format("mkdir -p {0} && cp {1} {2} && chown 0:0 {2} && chmod 755 {2} && mv -f {2} {3} || {{ rm -f {2}; exit 1; }}",
                         StringUtils::ShellQuote(CLI_DIR.string()), StringUtils::ShellQuote(cliSrc.string()),
                         StringUtils::ShellQuote(stagingPath.string()), StringUtils::ShellQuote(cliPath.string()));
  if(Shell::RunCommand(cmd).exitCode != 0)
    m_Logger(I18n::Get("error_file_write", cliPath.string()));
}

void ServiceInstaller::UninstallCli() {
  auto cliPath = CLI_DIR / CLI_FILE;
  if(!boost::filesystem::exists(cliPath))
    return;
  m_Logger("Removing CLI...");
  if(!Shell::Remove(cliPath.string()))
    m_Logger(I18n::Get("error_file_remove", cliPath.string()));
}
