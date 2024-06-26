#include "LogsWindow.h"

#include <spdlog/spdlog.h>

#include "shell/Shell.h"
#include "storage/AppSettings.h"

QString LogsWindow::GetDesktopLogs() {
    spdlog::default_logger()->flush();
    auto result = Shell::ReadBytes(AppSettings::BASE_DIR / "desktop.log");
    return QString::fromUtf8(result);
}

QString LogsWindow::GetModuleLogs() {
    auto result = Shell::ReadBytes(AppSettings::BASE_DIR / "module.log");
    return QString::fromUtf8(result);
}
