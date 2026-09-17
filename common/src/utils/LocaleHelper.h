#ifndef LOCALEHELPER_H
#define LOCALEHELPER_H

#include <mutex>
#include <optional>
#include <string>

class LocaleHelper {
public:
  enum class Locale {
    ENGLISH,
    GERMAN,
    CHINESE_SIMPLIFIED,
    PORTUGUESE_PT,
    PORTUGUESE_BR,
  };

  static Locale GetUserLocale();
  static std::string ToString(Locale locale);

private:
  LocaleHelper() = default;

  static Locale DetectSystemLocale();

  static std::optional<Locale> g_SystemLocale;
  static std::mutex g_Mutex;
};

#endif
