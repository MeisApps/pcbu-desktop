#include "I18n.h"

#include <nlohmann/json.hpp>

#include "ResourceHelper.h"
#include "utils/LocaleHelper.h"

std::string I18n::Get(const std::string &key) {
    try {
        std::string i18nPath{};
        switch (LocaleHelper::GetUserLocale()) {
            case LocaleHelper::Locale::GERMAN:
                i18nPath = ":/res/i18n/de_DE.json";
                break;
            case LocaleHelper::Locale::ENGLISH:
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

