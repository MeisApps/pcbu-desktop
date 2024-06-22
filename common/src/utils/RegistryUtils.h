#ifndef PCBU_DESKTOP_REGISTRYUTILS_H
#define PCBU_DESKTOP_REGISTRYUTILS_H

#include <string>
#include <vector>
#include <Windows.h>

class RegistryUtils {
public:
    static std::string GetStringValue(HKEY hKeyParent, const std::string& subKey, const std::string& valueName);

private:
    RegistryUtils() = default;
};


#endif //PCBU_DESKTOP_REGISTRYUTILS_H
