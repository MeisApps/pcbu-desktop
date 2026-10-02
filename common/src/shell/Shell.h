#ifndef PCBU_DESKTOP_SHELL_H
#define PCBU_DESKTOP_SHELL_H

#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "ElevatorCommands.h"

class ElevatorService;

#ifdef WINDOWS
#undef CreateFile
#endif

class Shell {
public:
  static void Init();
  static void Destroy();

  static bool HasAdmin();

  static void SetElevatorLostHandler(const std::function<void()> &handler);

  static ShellCmdResult RunCommand(const std::string &cmd);

  static bool CreateDir(const std::filesystem::path &path);
  static bool CreateFile(const std::filesystem::path &path);
  static bool Remove(const std::filesystem::path &path);

  static std::vector<uint8_t> ReadBytes(const std::filesystem::path &path);
  static bool WriteBytes(const std::filesystem::path &path, const std::vector<uint8_t> &data);

  static bool ProtectFile(const std::filesystem::path &path, bool enabled);

private:
  Shell() = default;

  static ElevatorService *GetElevator();
  static std::optional<ElevatorCommandResponse> Exec(ElevatorService &elevator, ElevatorCommandType type, std::vector<std::string> args,
                                                     std::vector<uint8_t> data = {});
  static void OnElevatorLost();

  static std::atomic<ElevatorService *> g_ElevatorService;
  static std::function<void()> g_ElevatorLostHandler;
  static std::mutex g_ElevatorLostMutex;
};

#endif // PCBU_DESKTOP_SHELL_H
