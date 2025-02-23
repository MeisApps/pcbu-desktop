#ifndef PCBU_DESKTOP_RESOURCEHELPER_H
#define PCBU_DESKTOP_RESOURCEHELPER_H

#include <spdlog/spdlog.h>
#include <vector>

class ResourceHelper {
public:
  template <typename... T> static std::vector<uint8_t> GetResource(const std::string &url, T &&...args) {
    return GetResource(fmt::format(fmt::runtime(url), args...));
  }

  static std::vector<uint8_t> GetResource(const std::string &url);

private:
  ResourceHelper() = default;
};

#endif // PCBU_DESKTOP_RESOURCEHELPER_H
