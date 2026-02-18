#ifndef LOCALSHELL_H
#define LOCALSHELL_H

#include <filesystem>

#include "ElevatorCommands.h"

class LocalShell {
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

  static bool ProtectFile(const std::filesystem::path &path, bool enabled);

private:
  LocalShell() = default;

#ifdef WINDOWS
  static bool ModifyFileAccess(const std::string &filePath, const std::string &sid, bool deny);
#endif
};

#endif
