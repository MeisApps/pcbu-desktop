#ifndef PCBU_DESKTOP_SERVICEINSTALLER_H
#define PCBU_DESKTOP_SERVICEINSTALLER_H

#include <string>
#include <functional>
#include <spdlog/spdlog.h>

#include "utils/I18n.h"
#if defined(LINUX) || defined(APPLE)
#include "PAMHelper.h"
#endif

struct ServiceSetting {
    std::string id{};
    std::string name{};
    bool enabled{};
    bool defaultVal{};
};

class ServiceInstaller {
public:
    explicit ServiceInstaller(const std::function<void(const std::string&)>& logCallback = [](const std::string& str){ spdlog::info(str); });

    std::vector<ServiceSetting> GetSettings();
    void ApplySettings(const std::vector<ServiceSetting>& settings, bool useDefault);
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
    static bool IsProgramInstalled(const std::string& pathName);

    std::function<void(const std::string&)> m_Logger{};
#if defined(LINUX) || defined(APPLE)
    PAMHelper m_PAMHelper{};
#endif
};


#endif //PCBU_DESKTOP_SERVICEINSTALLER_H
