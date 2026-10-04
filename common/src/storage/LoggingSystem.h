#ifndef PCBU_DESKTOP_LOGGINGSYSTEM_H
#define PCBU_DESKTOP_LOGGINGSYSTEM_H

#include <spdlog/spdlog.h>

class LoggingSystem {
public:
  static void Init(const std::string &logName, bool printToConsole = true, bool writeToFile = true);
  static void Destroy();

private:
  LoggingSystem() = default;
  static std::string g_LogName;

  static constexpr const char *LOGGER_NAME = "pcbu_logger";
  static constexpr const char *NULL_LOGGER_NAME = "pcbu_null_logger";
};

#endif // PCBU_DESKTOP_LOGGINGSYSTEM_H
