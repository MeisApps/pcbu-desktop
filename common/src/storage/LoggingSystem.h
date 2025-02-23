#ifndef PCBU_DESKTOP_LOGGINGSYSTEM_H
#define PCBU_DESKTOP_LOGGINGSYSTEM_H

#include <spdlog/spdlog.h>

class LoggingSystem {
public:
  static void Init(const std::string &logName, bool printToConsole = true);
  static void Destroy();

private:
  LoggingSystem() = default;
  static std::string g_LogName;
};

#endif // PCBU_DESKTOP_LOGGINGSYSTEM_H
