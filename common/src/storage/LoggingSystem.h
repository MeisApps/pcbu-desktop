#ifndef PCBU_DESKTOP_LOGGINGSYSTEM_H
#define PCBU_DESKTOP_LOGGINGSYSTEM_H

#include <spdlog/sinks/dist_sink.h>
#include <spdlog/spdlog.h>

class LoggingSystem {
public:
  static void Init(const std::string &logName, bool printToConsole = true, bool writeToFile = true);
  static void Destroy();

  static void SetWriteToFile(bool writeToFile);

  static bool IsDebugEnabled();
  static bool SetDebugEnabled(bool enabled);

private:
  LoggingSystem() = default;

  static spdlog::sink_ptr CreateFileSink(spdlog::level::level_enum logLevel);

  static std::string g_LogName;
  static std::shared_ptr<spdlog::sinks::dist_sink_mt> g_FileSink;

  static constexpr const char *LOGGER_NAME = "pcbu_logger";
  static constexpr const char *NULL_LOGGER_NAME = "pcbu_null_logger";
  static constexpr const char *LOG_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%l] %v";
};

#endif // PCBU_DESKTOP_LOGGINGSYSTEM_H
