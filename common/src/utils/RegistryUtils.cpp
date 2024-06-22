#include "RegistryUtils.h"

#include <spdlog/spdlog.h>

std::string RegistryUtils::GetStringValue(HKEY hKeyParent, const std::string &subKey, const std::string &valueName) {
    HKEY hKey;
    LONG result = RegOpenKeyExA(hKeyParent, subKey.c_str(), 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        spdlog::error("Failed to open registry key. (Status={})", result);
        return {};
    }
    DWORD dataSize = 0;
    result = RegQueryValueExA(hKey, valueName.c_str(), nullptr, nullptr, nullptr, &dataSize);
    if (result != ERROR_SUCCESS) {
        spdlog::error("Failed to query registry value. (Status={})", result);
        RegCloseKey(hKey);
        return {};
    }
    std::vector<char> data(dataSize);
    result = RegQueryValueExA(hKey, valueName.c_str(), nullptr, nullptr, reinterpret_cast<BYTE *>(data.data()), &dataSize);
    if (result != ERROR_SUCCESS) {
        spdlog::error("Failed to query registry value. (Status={})", result);
        RegCloseKey(hKey);
        return {};
    }

    RegCloseKey(hKey);
    if (!data.empty() && data.back() == '\0')
        data.pop_back();
    return {data.begin(), data.end()};
}
