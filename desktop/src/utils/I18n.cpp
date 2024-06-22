#include "I18n.h"

#include <clocale>
#include <nlohmann/json.hpp>

#include "ResourceHelper.h"
#include "storage/AppSettings.h"
#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <Windows.h>
#endif

std::string I18n::Get(const std::string &key) {
    try {
        std::string i18nPath{};
        switch (GetLocale()) {
            case I18n::Locale::GERMAN:
                i18nPath = ":/res/i18n/de_DE.json";
                break;
            case I18n::Locale::ENGLISH:
            default:
                i18nPath = ":/res/i18n/en_US.json";
                break;
        }
        auto i18nFile = ResourceHelper::GetResource(i18nPath);
        nlohmann::json json = nlohmann::json::parse(i18nFile);
        auto langMap = json.get<std::map<std::string, std::string>>();
        if(langMap.contains(key))
            return langMap[key];
        spdlog::warn("Key {} not found in i18n.", key);
    } catch (const std::exception& ex) {
        spdlog::warn("Failed to parse i18n. {}", ex.what());
    }
    return key;
}

I18n::Locale I18n::GetLocale() {
    auto settingsLang = AppSettings::Get().language;
    if(settingsLang == "en")
        return I18n::Locale::ENGLISH;
    if(settingsLang == "de")
        return I18n::Locale::GERMAN;

    std::string systemLocale{};
#ifdef WINDOWS
    WCHAR localeName[LOCALE_NAME_MAX_LENGTH];
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) == 0) {
        spdlog::error("Failed to get system locale.");
        return I18n::Locale::UNKNOWN;
    }
    systemLocale = StringUtils::FromWideString(localeName);
#else
    if(auto lcSys = std::setlocale(LC_ALL, ""); lcSys && *lcSys)
        systemLocale = lcSys;
    if (auto lcAll = std::getenv("LC_ALL"); lcAll && *lcAll)
        systemLocale = lcAll;
    if (auto lcCType = std::getenv("LC_CTYPE"); lcCType && *lcCType)
        systemLocale = lcCType;
    if (auto lang = std::getenv("LANG"); lang && *lang)
        systemLocale = lang;
#endif

    if(systemLocale.starts_with("de"))
        return I18n::Locale::GERMAN;
    return I18n::Locale::ENGLISH;
}
