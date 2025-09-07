#ifndef LOCALEHELPER_H
#define LOCALEHELPER_H

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
};

#endif
