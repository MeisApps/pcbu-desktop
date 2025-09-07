#include "Shell.h"

#include <boost/filesystem.hpp>
#include <boost/process/v1.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <boost/process/v1/windows.hpp>

#define SHELL_NAME "cmd.exe"
#define SHELL_CMD_ARG "/c"
#elif LINUX
#define SHELL_NAME "bash"
#define SHELL_CMD_ARG "-c"
#elif APPLE
#define SHELL_NAME "zsh"
#define SHELL_CMD_ARG "-c"
#endif

bool Shell::IsRunningAsAdmin() {
#ifdef WINDOWS
  BOOL isAdmin = FALSE;
  PSID adminGroup = nullptr;
  SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
  if(!AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
    spdlog::error("AllocateAndInitializeSid failed. (Code={})", GetLastError());
    return false;
  }
  if(!CheckTokenMembership(nullptr, adminGroup, &isAdmin)) {
    spdlog::error("CheckTokenMembership failed. (Code={})", GetLastError());
    isAdmin = FALSE;
  }
  if(adminGroup)
    FreeSid(adminGroup);
  return isAdmin;
#else
  return geteuid() == 0;
#endif
}

ShellCmdResult Shell::RunCommand(const std::string &cmd) {
  /*boost::process::child proc(boost::process::search_path("osascript"), ToDo
                             std::vector<std::string> {"-e", "do shell script \"echo test\" with administrator privileges"},
                             boost::process::std_out > outStream);*/
  return RunUserCommand(cmd);
}

ShellCmdResult Shell::RunUserCommand(const std::string &cmd) {
  boost::process::v1::ipstream outStream{};
  boost::process::v1::ipstream errStream{};
#ifdef WINDOWS
  boost::process::v1::child proc(fmt::format("{0} {1} \"{2}\"", SHELL_NAME, SHELL_CMD_ARG, cmd), boost::process::v1::std_out > outStream,
                                 boost::process::v1::std_err > errStream, boost::process::v1::windows::create_no_window);
#else
  boost::process::v1::child proc(fmt::format("{0} {1} \"{2}\"", SHELL_NAME, SHELL_CMD_ARG, cmd), boost::process::v1::std_out > outStream,
                                 boost::process::v1::std_err > errStream);
#endif
  std::string output{};
  std::string line{};
  while(outStream && std::getline(outStream, line) && !line.empty())
    output.append(line + "\n");
  while(errStream && std::getline(errStream, line) && !line.empty())
    output.append(line + "\n");
  proc.wait();

  spdlog::debug("Process exit. Code: {} Command: '{}' Output: '{}'", proc.exit_code(), cmd, output);
  auto result = ShellCmdResult();
  result.exitCode = proc.exit_code();
  result.output = output;
  return result;
}

void Shell::SpawnCommand(const std::string &cmd) {
#ifdef WINDOWS
  boost::process::v1::child proc(fmt::format("{0} {1} \"{2}\"", SHELL_NAME, SHELL_CMD_ARG, cmd), boost::process::windows::create_no_window);
#else
  boost::process::v1::child proc(fmt::format("{0} {1} \"{2}\"", SHELL_NAME, SHELL_CMD_ARG, cmd));
#endif
  proc.detach();
}

bool Shell::CreateDir(const std::filesystem::path &path) {
  boost::system::error_code ec{};
  boost::filesystem::create_directories(boost::filesystem::path(path), ec);
  return !ec.failed();
  /*#ifdef WINDOWS
      return RunCommand(fmt::format("mkdir \"{}\"", path.string())).exitCode == 0;
  #else
      return RunCommand(fmt::format("mkdir -p \"{}\"", path.string())).exitCode == 0;
  #endif*/
}

bool Shell::RemoveDir(const std::filesystem::path &path) {
  boost::system::error_code ec{};
  boost::filesystem::remove(boost::filesystem::path(path), ec);
  return !ec.failed();
  /*#ifdef WINDOWS
      return RunCommand(fmt::format("rd /s /q \"{}\"", path.string())).exitCode == 0;
  #else
      return RunCommand(fmt::format("rm -R \"{}\"", path.string())).exitCode == 0;
  #endif*/
}

bool Shell::RemoveFile(const std::filesystem::path &path) {
  boost::system::error_code ec{};
  boost::filesystem::remove(boost::filesystem::path(path), ec);
  return !ec.failed();
  /*#ifdef WINDOWS
      return RunCommand(fmt::format("del \"{}\"", path.string())).exitCode == 0;
  #else
      return RunCommand(fmt::format("rm \"{}\"", path.string())).exitCode == 0;
  #endif*/
}

std::vector<uint8_t> Shell::ReadBytes(const std::filesystem::path &path) {
  std::ifstream file{};
  file.open(path, std::ios_base::binary);
  if(!file) {
    spdlog::error("Failed to open file '{}'.", path.string());
    return {};
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

bool Shell::WriteBytes(const std::filesystem::path &path, const std::vector<uint8_t> &data) {
  std::ofstream file(path, std::ios::out | std::ios::binary);
  file.write(reinterpret_cast<const char *>(data.data()), (std::streamsize)data.size());
  file.close();
  return !file.fail() && !file.bad();
}
