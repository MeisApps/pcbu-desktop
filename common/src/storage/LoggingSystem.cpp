#include "LoggingSystem.h"

#include <filesystem>
#include <fstream>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <vector>

#include "AppSettings.h"
#include "shell/LocalShell.h"

std::string LoggingSystem::g_LogName{};

void LoggingSystem::Init(const std::string &logName, bool printToConsole, bool writeToFile) {
  g_LogName = logName;
  auto logsDir = AppSettings::GetLogsDir(!LocalShell::IsRunningAsAdmin());
  auto logPath = logsDir / fmt::format("{}.log", g_LogName);
  if(writeToFile) {
    if(!std::filesystem::exists(logsDir))
      LocalShell::CreateDir(logsDir);
    std::ifstream logFile(logPath, std::ifstream::ate | std::ifstream::binary);
    if(logFile) {
      auto sizeKb = logFile.tellg() / 1000;
      if(sizeKb > 5000)
        LocalShell::Remove(logPath);
    }
  }

  try {
    auto enableDebug = std::filesystem::exists(AppSettings::GetBaseDir() / "LOG_DEBUG");
    auto logLevel = enableDebug ? spdlog::level::debug : spdlog::level::info;

    std::vector<spdlog::sink_ptr> sinks{};
    if(printToConsole) {
      auto consoleSink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
      consoleSink->set_level(logLevel);
      sinks.push_back(consoleSink);
    }
    if(writeToFile) {
      auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), false);
      fileSink->set_level(logLevel);
      sinks.push_back(fileSink);
    }

    auto loggerPtr = std::make_shared<spdlog::logger>(LOGGER_NAME, sinks.begin(), sinks.end());
    loggerPtr->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    loggerPtr->set_level(logLevel);
    spdlog::set_default_logger(loggerPtr);
    spdlog::flush_on(logLevel);
    spdlog::info("Logger init.");
  } catch(const std::exception &ex) {
    spdlog::error("Error initializing logger: {}", ex.what());
  }
}

void LoggingSystem::Destroy() {
  spdlog::info("Logger destroy.");
  auto nullLogger = std::make_shared<spdlog::logger>(NULL_LOGGER_NAME);
  nullLogger->set_level(spdlog::level::off);
  spdlog::set_default_logger(nullLogger);
  spdlog::drop(LOGGER_NAME);
  g_LogName = {};
}
