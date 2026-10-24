#ifndef PCBU_DESKTOP_RESOURCEHELPER_H
#define PCBU_DESKTOP_RESOURCEHELPER_H

#include <cstdint>
#include <span>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

class ResourceHelper {
public:
  template <typename... T> static std::vector<uint8_t> GetResource(const std::string &url, T &&...args) {
    return GetResource(fmt::format(fmt::runtime(url), args...));
  }

  static std::vector<uint8_t> GetResource(const std::string &url);

private:
  ResourceHelper() = default;

  static std::span<const uint8_t> GetResourceData(const std::string &url);
#ifdef WINDOWS
  static std::span<const uint8_t> LoadRcData(int id);
#endif
};

#endif // PCBU_DESKTOP_RESOURCEHELPER_H
