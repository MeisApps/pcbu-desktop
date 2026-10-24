#include "TerminalUtils.h"

#include <charconv>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <spdlog/spdlog.h>
#include <thread>

#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <Windows.h>
#include <io.h>
#else
#include <csignal>
#include <termios.h>
#include <unistd.h>
#endif

std::atomic<bool> TerminalUtils::g_IsInterruptible{};
std::atomic<bool> TerminalUtils::g_Interrupted{};

std::atomic<bool> TerminalUtils::g_IsEchoDisabled{};
#ifdef WINDOWS
unsigned long TerminalUtils::g_SavedConsoleMode{};
#else
termios TerminalUtils::g_SavedTerminal{};
#endif

void TerminalUtils::Init() {
#ifdef WINDOWS
  SetConsoleOutputCP(CP_UTF8);
  auto outHandle = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD outMode{};
  if(GetConsoleMode(outHandle, &outMode))
    SetConsoleMode(outHandle, outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  SetConsoleCtrlHandler(
      [](DWORD type) -> BOOL {
        if(type != CTRL_C_EVENT && type != CTRL_BREAK_EVENT)
          return FALSE;
        OnInterrupt(130);
        return TRUE;
      },
      TRUE);
#else
  struct sigaction action{};
  action.sa_handler = [](int sig) { OnInterrupt(128 + sig); };
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_RESTART;
  sigaction(SIGINT, &action, nullptr);
  sigaction(SIGTERM, &action, nullptr);
#endif
}

bool TerminalUtils::IsInteractive() {
#ifdef WINDOWS
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0;
#endif
}

std::optional<std::string> TerminalUtils::ReadLine(const std::string &prompt) {
  std::cout << prompt << std::flush;
  std::string line{};
  if(!std::getline(std::cin, line))
    return {};
  return StringUtils::Trim(line);
}

std::optional<std::string> TerminalUtils::ReadPassword(const std::string &prompt) {
  auto hasConsole = DisableEcho();
  std::cout << prompt << std::flush;
  std::string line{};
  auto ok = static_cast<bool>(std::getline(std::cin, line));
  RestoreEcho();
  if(hasConsole)
    std::cout << std::endl;
  if(!ok)
    return {};
  return line;
}

std::optional<size_t> TerminalUtils::Choose(const std::string &prompt, const std::vector<std::string> &options, size_t defaultIndex) {
  std::cout << prompt << std::endl;
  for(size_t i = 0; i < options.size(); i++)
    std::cout << fmt::format("  {}) {}", i + 1, options[i]) << std::endl;
  while(true) {
    auto line = ReadLine(fmt::format("Select [{}]: ", defaultIndex + 1));
    if(!line.has_value())
      return {};
    if(line->empty())
      return defaultIndex;
    size_t index{};
    auto [ptr, ec] = std::from_chars(line->data(), line->data() + line->size(), index);
    if(ec == std::errc() && ptr == line->data() + line->size() && index >= 1 && index <= options.size())
      return index - 1;
    std::cout << fmt::format("Please enter a number between 1 and {}.", options.size()) << std::endl;
  }
}

bool TerminalUtils::Confirm(const std::string &prompt) {
  auto line = ReadLine(prompt + " [y/N]: ");
  if(!line.has_value())
    return false;
  auto answer = StringUtils::ToLower(line.value());
  return answer == "y" || answer == "yes";
}

bool TerminalUtils::RunInterruptible(const std::function<void(std::atomic<bool> &isRunning)> &task) {
  std::atomic<bool> isRunning{true};
  std::atomic<bool> isDone{};
  std::exception_ptr exception{};
  g_Interrupted = false;
  g_IsInterruptible = true;
  std::thread thread([&]() {
    try {
      task(isRunning);
    } catch(...) {
      exception = std::current_exception();
    }
    isDone = true;
  });
  while(!isDone) {
    if(g_Interrupted)
      isRunning = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  thread.join();
  g_IsInterruptible = false;
  if(exception)
    std::rethrow_exception(exception);
  return !g_Interrupted;
}

void TerminalUtils::OnInterrupt(int exitCode) {
  if(g_IsInterruptible && !g_Interrupted.exchange(true))
    return;
  RestoreEcho();
  _exit(exitCode);
}

bool TerminalUtils::DisableEcho() {
#ifdef WINDOWS
  auto inHandle = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode{};
  if(!GetConsoleMode(inHandle, &mode))
    return false;
  g_SavedConsoleMode = mode;
  g_IsEchoDisabled = true;
  SetConsoleMode(inHandle, mode & ~ENABLE_ECHO_INPUT);
#else
  if(tcgetattr(STDIN_FILENO, &g_SavedTerminal) != 0)
    return false;
  g_IsEchoDisabled = true;
  auto newTerminal = g_SavedTerminal;
  newTerminal.c_lflag &= ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &newTerminal);
#endif
  return true;
}

void TerminalUtils::RestoreEcho() {
  if(!g_IsEchoDisabled.exchange(false))
    return;
#ifdef WINDOWS
  SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), g_SavedConsoleMode);
#else
  tcsetattr(STDIN_FILENO, TCSANOW, &g_SavedTerminal);
#endif
}
