#ifndef PCBU_DESKTOP_SHELL_H
#define PCBU_DESKTOP_SHELL_H

#include <filesystem>
#include <string>
#include <vector>

struct ShellCmdResult {
  int exitCode{};
  std::string output{};
};

class Shell {
public:
  static bool IsRunningAsAdmin();

  static ShellCmdResult RunCommand(const std::string &cmd);
  static ShellCmdResult RunUserCommand(const std::string &cmd);

  static void SpawnCommand(const std::string &cmd);

  static bool CreateDir(const std::filesystem::path &path);
  static bool RemoveDir(const std::filesystem::path &path);
  static bool RemoveFile(const std::filesystem::path &path);

  static std::vector<uint8_t> ReadBytes(const std::filesystem::path &path);
  static bool WriteBytes(const std::filesystem::path &path, const std::vector<uint8_t> &data);

private:
  Shell() = default;
};

#endif // PCBU_DESKTOP_SHELL_H
