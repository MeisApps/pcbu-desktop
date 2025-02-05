#include "LoggingSystem.h"

#include <fstream>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "AppSettings.h"
#include "shell/Shell.h"

std::string LoggingSystem::g_LogName{};

void LoggingSystem::Init(const std::string &logName, bool printToConsole) {
    g_LogName = logName;
    auto logPath = AppSettings::GetBaseDir() / fmt::format("{}.log", g_LogName);
    std::ifstream logFile(logPath, std::ifstream::ate | std::ifstream::binary);
    if(logFile) {
        auto sizeKb = logFile.tellg() / 1000;
        if(sizeKb > 5000)
            Shell::RemoveFile(logPath);
    }

    try {
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), false);
        std::shared_ptr<spdlog::logger> loggerPtr{};
        if(printToConsole) {
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
            auto logger = spdlog::logger("pcbu_logger", {consoleSink, fileSink});
            loggerPtr = std::make_shared<spdlog::logger>(logger);
        } else {
            auto logger = spdlog::logger("pcbu_logger", {fileSink});
            loggerPtr = std::make_shared<spdlog::logger>(logger);
        }
        loggerPtr->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
        spdlog::set_default_logger(loggerPtr);
#ifdef _DEBUG
        spdlog::flush_on(spdlog::level::debug);
#else
        spdlog::flush_on(spdlog::level::warn);
#endif
        spdlog::info("Logger init.");
    } catch(const std::exception& ex) {
        spdlog::error("Error initializing logger: {}", ex.what());
    }
}

void LoggingSystem::Destroy() {
    spdlog::info("Logger destroy.");
    spdlog::shutdown();
    g_LogName = {};
}
