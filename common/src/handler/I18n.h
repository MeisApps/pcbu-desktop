#ifndef PAM_PCBIOUNLOCK_I18N_H
#define PAM_PCBIOUNLOCK_I18N_H

#include <spdlog/spdlog.h>

class I18n {
public:
    template<typename ...T>
    static std::string Get(const std::string& key, T&&... args) {
        return fmt::format(fmt::runtime(Get(key)), args...);
    }

    static std::string Get(const std::string& key);

private:
    I18n() = default;
};

#endif //PAM_PCBIOUNLOCK_I18N_H
