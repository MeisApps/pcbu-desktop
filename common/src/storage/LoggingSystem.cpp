#include "LoggingSystem.h"

#include <filesystem>
#include <fstream>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <vector>

#include "AppSettings.h"
#include "shell/LocalShell.h"
#include "shell/Shell.h"

std::string LoggingSystem::g_LogName{};
std::shared_ptr<spdlog::sinks::dist_sink_mt> LoggingSystem::g_FileSink{};

void LoggingSystem::Init(const std::string &logName, bool printToConsole, bool writeToFile) {
  g_LogName = logName;
  try {
    auto enableDebug = IsDebugEnabled();
    auto logLevel = enableDebug ? spdlog::level::debug : spdlog::level::info;

    std::vector<spdlog::sink_ptr> sinks{};
    if(printToConsole) {
      auto consoleSink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
      consoleSink->set_level(logLevel);
      sinks.push_back(consoleSink);
    }
    g_FileSink = std::make_shared<spdlog::sinks::dist_sink_mt>();
    g_FileSink->set_level(logLevel);
    if(writeToFile)
      g_FileSink->add_sink(CreateFileSink(logLevel));
    sinks.push_back(g_FileSink);

    auto loggerPtr = std::make_shared<spdlog::logger>(LOGGER_NAME, sinks.begin(), sinks.end());
    loggerPtr->set_pattern(LOG_PATTERN);
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
  g_FileSink = {};
  g_LogName = {};
}

void LoggingSystem::SetWriteToFile(bool writeToFile) {
  if(!g_FileSink)
    return;

  if(!writeToFile) {
    g_FileSink->set_sinks({});
    return;
  }
  try {
    g_FileSink->set_sinks({CreateFileSink(g_FileSink->level())});
  } catch(const std::exception &ex) {
    spdlog::error("Error creating log file: {}", ex.what());
  }
}

bool LoggingSystem::IsDebugEnabled() {
  return std::filesystem::exists(AppSettings::GetBaseDir() / "LOG_DEBUG");
}

bool LoggingSystem::SetDebugEnabled(bool enabled) {
  if(enabled)
    return Shell::CreateFile(AppSettings::GetBaseDir() / "LOG_DEBUG");
  return Shell::Remove(AppSettings::GetBaseDir() / "LOG_DEBUG");
}

spdlog::sink_ptr LoggingSystem::CreateFileSink(spdlog::level::level_enum logLevel) {
  auto logsDir = AppSettings::GetLogsDir(!LocalShell::IsRunningAsAdmin());
  auto logPath = logsDir / fmt::format("{}.log", g_LogName);
  if(!std::filesystem::exists(logsDir))
    LocalShell::CreateDir(logsDir);
  std::ifstream logFile(logPath, std::ifstream::ate | std::ifstream::binary);
  if(logFile) {
    auto sizeKb = logFile.tellg() / 1000;
    if(sizeKb > 5000)
      LocalShell::Remove(logPath);
  }

  auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), false);
  fileSink->set_level(logLevel);
  fileSink->set_pattern(LOG_PATTERN);
  return fileSink;
}
