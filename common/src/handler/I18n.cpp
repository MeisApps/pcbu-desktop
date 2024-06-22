#include "I18n.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "storage/AppSettings.h"

#ifdef _WIN32
#pragma warning(disable : 4244)
#include <Windows.h>
#endif

int I18n::m_Locale = -1;

void I18n::FindLocale() {
    auto settingsLang = AppSettings::Get().language;
    if(settingsLang != "auto") {
        if(settingsLang == "de_DE")
            m_Locale = 1;
        else
            m_Locale = 0;
        return;
    }

#ifdef _WIN32
    LCID lcid = GetThreadLocale();
    wchar_t name[LOCALE_NAME_MAX_LENGTH];
    if (LCIDToLocaleName(lcid, name, LOCALE_NAME_MAX_LENGTH, 0) == 0) {
        m_Locale = 0;
        return;
    }

    auto ws = std::wstring(name);
    auto locale = std::string(ws.begin(), ws.end());
#else
    std::string locale;
    if(std::filesystem::exists("/etc/locale.conf")) {
        std::ifstream is_file("/etc/locale.conf");
        std::string line;
        while(std::getline(is_file, line)) {
            std::istringstream is_line(line);
            std::string key;
            if(std::getline(is_line, key, '=')) {
                std::string value;
                if(std::getline(is_line, value)) {
                    if(key == "LANG") {
                        locale = value;
                        break;
                    }
                }
            }
        }
    } else {
        auto lang = setlocale(LC_ALL, nullptr);
        locale = std::string(lang);
    }
#endif

    if(locale.starts_with("de"))
        m_Locale = 1;
    else
        m_Locale = 0;
}
