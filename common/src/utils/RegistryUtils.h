#ifndef PCBU_DESKTOP_REGISTRYUTILS_H
#define PCBU_DESKTOP_REGISTRYUTILS_H

#include <Windows.h>
#include <optional>
#include <string>
#include <vector>

class RegistryUtils {
public:
  static std::optional<std::string> GetStringValue(HKEY hKeyParent, const std::string &subKey, const std::string &valueName);
  static bool SetStringValue(HKEY hKeyParent, const std::string &subKey, const std::string &valueName, const std::string &newValue);

private:
  RegistryUtils() = default;
};

#endif // PCBU_DESKTOP_REGISTRYUTILS_H
