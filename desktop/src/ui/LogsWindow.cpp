#include "LogsWindow.h"

#include <spdlog/spdlog.h>

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
        auto desktopLogs = Shell::ReadBytes(AppSettings::BASE_DIR / "desktop.log");
        auto moduleLogs = Shell::ReadBytes(AppSettings::BASE_DIR / "module.log");
        QMetaObject::invokeMethod(window, "setLogs", Q_ARG(QVariant, QString::fromUtf8(desktopLogs)), Q_ARG(QVariant, QString::fromUtf8(moduleLogs)));
    });
}
