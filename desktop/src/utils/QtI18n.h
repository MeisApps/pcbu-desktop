#ifndef PCBU_DESKTOP_QTI18N_H
#define PCBU_DESKTOP_QTI18N_H

#include <spdlog/fmt/fmt.h>

class QtI18n {
public:
    template<typename ...T>
    static std::string Get(const std::string& key, T&&... args) {
        return fmt::format(fmt::runtime(Get(key)), args...);
    }

    static std::string Get(const std::string& key);

private:
    QtI18n() = default;
};

#endif //PCBU_DESKTOP_QTI18N_H
