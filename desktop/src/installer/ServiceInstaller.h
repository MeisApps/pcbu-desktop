#ifndef PCBU_DESKTOP_SERVICEINSTALLER_H
#define PCBU_DESKTOP_SERVICEINSTALLER_H

#include <functional>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

#include "utils/I18n.h"
#if defined(LINUX) || defined(APPLE)
#include "PAMHelper.h"
#endif

struct ServiceSettingOption {
  std::string value{};
  std::string name{};
};

struct ServiceSetting {
  std::string type{};
  std::string id{};
  std::string name{};

  // Toggle
  bool enabled{};
  bool defaultVal{};

  // Choice
  std::vector<ServiceSettingOption> options{};
  std::string selectedValue{};
  std::string defaultValue{};

  // Toggle setting
  ServiceSetting(std::string id, std::string name, bool enabled, bool defaultVal)
      : type("toggle"), id(std::move(id)), name(std::move(name)), enabled(enabled), defaultVal(defaultVal) {}

  // Choice setting
  ServiceSetting(std::string id, std::string name, std::vector<ServiceSettingOption> options, std::string selectedValue,
                 std::string defaultValue)
      : type("choice"), id(std::move(id)), name(std::move(name)), options(std::move(options)), selectedValue(std::move(selectedValue)),
        defaultValue(std::move(defaultValue)) {}

  ServiceSetting() = default;
};

class ServiceInstaller {
public:
  explicit ServiceInstaller(const std::function<void(const std::string &)> &logCallback = [](const std::string &str) { spdlog::info(str); });

  std::vector<ServiceSetting> GetSettings();
  void ApplySettings(const std::vector<ServiceSetting> &settings, bool useDefault);
  void ClearSettings() {
    auto settings = GetSettings();
    for(auto setting : settings)
      setting.enabled = false;
    ApplySettings(settings, false);
  }

  static bool IsInstalled();
  void Install();
  void Uninstall();

private:
  static bool IsProgramInstalled(const std::string &pathName);

  std::function<void(const std::string &)> m_Logger{};
#if defined(LINUX) || defined(APPLE)
  PAMHelper m_PAMHelper{};
#endif
};

#endif // PCBU_DESKTOP_SERVICEINSTALLER_H
