#ifndef LOCALSHELL_H
#define LOCALSHELL_H

#include <filesystem>

#include "ElevatorCommands.h"

#ifdef WINDOWS
#undef CreateFile
#endif

class LocalShell {
public:
  static bool IsRunningAsAdmin();

  static ShellCmdResult RunCommand(const std::string &cmd);
  static ShellCmdResult RunUserCommand(const std::string &cmd);
  static void SpawnCommand(const std::string &cmd);

  static bool CreateDir(const std::filesystem::path &path);
  static bool CreateFile(const std::filesystem::path &path);
  static bool Remove(const std::filesystem::path &path, bool recursive = false);

  static std::vector<uint8_t> ReadBytes(const std::filesystem::path &path);
  static bool WriteBytes(const std::filesystem::path &path, const std::vector<uint8_t> &data);

  static bool ProtectFile(const std::filesystem::path &path, bool enabled);

private:
  LocalShell() = default;

  static ShellCmdResult RunShell(const std::string &cmd, bool loadUserConfig);

#ifdef WINDOWS
  static bool ModifyFileAccess(const std::string &filePath, const std::string &sid, bool deny);
#endif
};

#endif
