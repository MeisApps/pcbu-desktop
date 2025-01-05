#ifndef LOCALEHELPER_H
#define LOCALEHELPER_H

class LocaleHelper {
public:
    enum class Locale {
        ENGLISH,
        GERMAN,
        CHINESE_SIMPLIFIED,
    };

    static Locale GetUserLocale();

private:
    LocaleHelper() = default;
};

#endif
