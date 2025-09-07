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
    if(settingsLang == "pt_PT")
      return Locale::PORTUGUESE_PT;
    if(settingsLang == "pt_BR")
      return Locale::PORTUGUESE_BR;
    return Locale::ENGLISH;
  }

#ifdef _WIN32
  WCHAR localeName[LOCALE_NAME_MAX_LENGTH];
  if(GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) == 0) {
    spdlog::error("Failed to get system locale.");
    return Locale::ENGLISH;
  }
  auto locale = StringUtils::FromWideString(localeName);
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
    if(auto lcAll = std::getenv("LC_ALL"); lcAll && *lcAll)
      locale = lcAll;
    if(auto lcCType = std::getenv("LC_CTYPE"); lcCType && *lcCType)
      locale = lcCType;
    if(auto lang = std::getenv("LANG"); lang && *lang)
      locale = lang;
  }
#endif

  locale = StringUtils::Replace(StringUtils::ToLower(locale), "-", "_");
  if(locale.starts_with("zh_cn"))
    return Locale::CHINESE_SIMPLIFIED;
  else if(locale.starts_with("de"))
    return Locale::GERMAN;
  else if(locale.starts_with("pt_pt"))
    return Locale::PORTUGUESE_PT;
  else if(locale.starts_with("pt_br"))
    return Locale::PORTUGUESE_BR;
  return Locale::ENGLISH;
}

std::string LocaleHelper::ToString(Locale locale) {
  switch(locale) {
    case Locale::ENGLISH:
      return "English";
    case Locale::GERMAN:
      return "German";
    case Locale::CHINESE_SIMPLIFIED:
      return "Chinese (Simplified)";
    case Locale::PORTUGUESE_PT:
      return "Portuguese";
    case Locale::PORTUGUESE_BR:
      return "Portuguese (Brazil)";
    default:
      return "Unknown";
  }
}
