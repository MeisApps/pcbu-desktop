#ifndef PCBU_DESKTOP_I18N_H
#define PCBU_DESKTOP_I18N_H

#include <spdlog/fmt/fmt.h>

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

#endif //PCBU_DESKTOP_I18N_H
