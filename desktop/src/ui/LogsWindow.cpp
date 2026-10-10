#include "LogsWindow.h"

#include <spdlog/spdlog.h>

#include "shell/LocalShell.h"
#include "shell/Shell.h"
#include "storage/AppSettings.h"

LogsWindow::~LogsWindow() {
  if(m_LoadThread.joinable())
    m_LoadThread.join();
}

void LogsWindow::LoadLogs(QObject *window) {
  if(m_LoadThread.joinable())
    m_LoadThread.join();
  m_LoadThread = std::thread([window]() {
    spdlog::default_logger()->flush();
    auto desktopLogs = LocalShell::ReadBytes(AppSettings::GetLogsDir(!LocalShell::IsRunningAsAdmin()) / "desktop.log");
    auto moduleLogs = Shell::ReadBytes(AppSettings::GetLogsDir(false) / "module.log");
    auto elevatorLogs = Shell::ReadBytes(AppSettings::GetLogsDir(false) / "elevator.log");
    std::vector<uint8_t> macAgentLogs{};
#ifdef APPLE
    macAgentLogs = LocalShell::ReadBytes(AppSettings::GetLogsDir(true) / "mac_agent.log");
    auto loginWindowLogs = Shell::ReadBytes(AppSettings::GetLogsDir(false) / "mac_agent.log");
    if(!loginWindowLogs.empty()) {
      std::string separator = "\n----- LoginWindow -----\n";
      macAgentLogs.insert(macAgentLogs.end(), separator.begin(), separator.end());
      macAgentLogs.insert(macAgentLogs.end(), loginWindowLogs.begin(), loginWindowLogs.end());
    }
#endif
    QMetaObject::invokeMethod(window, "setLogs", Q_ARG(QVariant, QString::fromUtf8(desktopLogs)), Q_ARG(QVariant, QString::fromUtf8(moduleLogs)),
                              Q_ARG(QVariant, QString::fromUtf8(elevatorLogs)), Q_ARG(QVariant, QString::fromUtf8(macAgentLogs)));
  });
}
