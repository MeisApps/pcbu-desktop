#ifndef PCBU_DESKTOP_REGISTRYUTILS_H
#define PCBU_DESKTOP_REGISTRYUTILS_H

#include <optional>
#include <string>
#include <vector>
#include <Windows.h>

class RegistryUtils {
public:
    static std::optional<std::string> GetStringValue(HKEY hKeyParent, const std::string& subKey, const std::string& valueName);
    static bool SetStringValue(HKEY hKeyParent, const std::string& subKey, const std::string& valueName, const std::string& newValue);

private:
    RegistryUtils() = default;
};


#endif //PCBU_DESKTOP_REGISTRYUTILS_H
