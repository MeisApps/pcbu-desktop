#ifndef PCBU_DESKTOP_APPINSTALLER_H
#define PCBU_DESKTOP_APPINSTALLER_H

#include <functional>
#include <spdlog/spdlog.h>
#include <string>

class AppInstaller {
public:
  explicit AppInstaller(const std::function<void(const std::string &)> &logCallback = [](const std::string &str) { spdlog::info(str); });

  void Install();
  void Uninstall();
  void Reinstall();

private:
  void Migrate();

  std::function<void(const std::string &)> m_Logger{};
};

#endif // PCBU_DESKTOP_APPINSTALLER_H
