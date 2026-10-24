#ifndef PCBU_CLI_TERMINALUTILS_H
#define PCBU_CLI_TERMINALUTILS_H

#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#ifndef WINDOWS
#include <termios.h>
#endif

class TerminalUtils {
public:
  static void Init();

  static bool IsInteractive();

  static std::optional<std::string> ReadLine(const std::string &prompt);
  static std::optional<std::string> ReadPassword(const std::string &prompt);

  static std::optional<size_t> Choose(const std::string &prompt, const std::vector<std::string> &options, size_t defaultIndex = 0);
  static bool Confirm(const std::string &prompt);

  static bool RunInterruptible(const std::function<void(std::atomic<bool> &isRunning)> &task);

private:
  TerminalUtils() = default;

  static void OnInterrupt(int exitCode);

  static bool DisableEcho();
  static void RestoreEcho();

  static std::atomic<bool> g_IsInterruptible;
  static std::atomic<bool> g_Interrupted;

  static std::atomic<bool> g_IsEchoDisabled;
#ifdef WINDOWS
  static unsigned long g_SavedConsoleMode;
#else
  static termios g_SavedTerminal;
#endif
};

#endif // PCBU_CLI_TERMINALUTILS_H
