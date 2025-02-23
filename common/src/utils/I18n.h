#ifndef I18N_H
#define I18N_H

#include <spdlog/spdlog.h>

class I18n {
public:
  template <typename... T> static std::string Get(const std::string &key, T &&...args) {
    return fmt::format(fmt::runtime(Get(key)), args...);
  }

  static std::string Get(const std::string &key);

private:
  I18n() = default;
};

#endif // I18N_H
