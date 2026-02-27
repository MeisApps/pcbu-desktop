#include "Shell.h"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/process/v2/environment.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/stdio.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <boost/process/v2/windows/default_launcher.hpp>
#undef CreateFile

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
  boost::asio::io_context ctx{};
  boost::asio::readable_pipe pipe{ctx};
#ifdef WINDOWS
  auto launcher = boost::process::v2::windows::default_launcher{};
  launcher.creation_flags() |= CREATE_NO_WINDOW;
#endif
  boost::process::v2::process proc(ctx, boost::process::v2::environment::find_executable(SHELL_NAME), {SHELL_CMD_ARG, cmd},
                                   boost::process::v2::process_stdio{{}, pipe, pipe}

#ifdef WINDOWS
                                   ,
                                   launcher
#endif
  );

  std::string output{};
  boost::system::error_code ec;
  boost::asio::read(pipe, boost::asio::dynamic_buffer(output), ec);
  proc.wait();

  spdlog::debug("Process exit. Code: {} Command: '{}' Output: '{}'", proc.exit_code(), cmd, StringUtils::Trim(output));
  auto result = ShellCmdResult();
  result.exitCode = proc.exit_code();
  result.output = output;
  return result;
}

void Shell::SpawnCommand(const std::string &cmd) {
  boost::asio::io_context ctx{};
#ifdef WINDOWS
  auto launcher = boost::process::v2::windows::default_launcher{};
  launcher.creation_flags() |= CREATE_NO_WINDOW;
#endif
  boost::process::v2::process proc(ctx, boost::process::v2::environment::find_executable(SHELL_NAME), {SHELL_CMD_ARG, cmd}
#ifdef WINDOWS
                                   ,
                                   launcher
#endif
  );
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

bool Shell::CreateFile(const std::filesystem::path &path) {
  std::ofstream file(path);
  return file.is_open();
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
