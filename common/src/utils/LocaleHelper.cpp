#include "LocaleHelper.h"

#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

#include "storage/AppSettings.h"
#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <Windows.h>
#endif

LocaleHelper::Locale LocaleHelper::GetUserLocale() {
    auto settingsLang = AppSettings::Get().language;
    if(settingsLang != "auto") {
        if(settingsLang == "zh_CN")
            return Locale::CHINESE_SIMPLIFIED;
        if(settingsLang == "de")
            return Locale::GERMAN;
        return Locale::ENGLISH;
    }

#ifdef _WIN32
    LCID lcid = GetThreadLocale();
    wchar_t name[LOCALE_NAME_MAX_LENGTH];
    if (LCIDToLocaleName(lcid, name, LOCALE_NAME_MAX_LENGTH, 0) == 0) {
        spdlog::warn("Failed to get thread locale.");
        return Locale::ENGLISH;
    }
    auto ws = std::wstring(name);
    auto locale = std::string(ws.begin(), ws.end());
#else
    std::string locale{};
    if(std::filesystem::exists("/etc/locale.conf")) {
        std::ifstream is_file("/etc/locale.conf");
        std::string line{};
        while(std::getline(is_file, line)) {
            std::istringstream is_line(line);
            std::string key{};
            if(std::getline(is_line, key, '=')) {
                std::string value{};
                if(std::getline(is_line, value)) {
                    if(key == "LANG") {
                        locale = value;
                        break;
                    }
                }
            }
        }
    }
    if(locale.empty()) {
        if(auto lcSys = std::setlocale(LC_ALL, ""); lcSys && *lcSys)
            locale = lcSys;
        if (auto lcAll = std::getenv("LC_ALL"); lcAll && *lcAll)
            locale = lcAll;
        if (auto lcCType = std::getenv("LC_CTYPE"); lcCType && *lcCType)
            locale = lcCType;
        if (auto lang = std::getenv("LANG"); lang && *lang)
            locale = lang;
    }
#endif

    locale = StringUtils::Replace(StringUtils::ToLower(locale), "-", "_");
    spdlog::info(locale);
    if(locale.starts_with("zh_cn"))
        return Locale::CHINESE_SIMPLIFIED;
    else if(locale.starts_with("de"))
        return Locale::GERMAN;
    return Locale::ENGLISH;
}
