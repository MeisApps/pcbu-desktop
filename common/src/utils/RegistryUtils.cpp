#include "RegistryUtils.h"

#include <spdlog/spdlog.h>

std::optional<std::string> RegistryUtils::GetStringValue(HKEY hKeyParent, const std::string &subKey, const std::string &valueName) {
  HKEY hKey;
  LONG result = RegOpenKeyExA(hKeyParent, subKey.c_str(), 0, KEY_READ, &hKey);
  if(result != ERROR_SUCCESS) {
    spdlog::error("Failed to open registry key. (Status={})", result);
    return {};
  }
  DWORD dataSize = 0;
  result = RegQueryValueExA(hKey, valueName.c_str(), nullptr, nullptr, nullptr, &dataSize);
  if(result != ERROR_SUCCESS) {
    spdlog::error("Failed to query registry value. (Status={})", result);
    RegCloseKey(hKey);
    return {};
  }
  std::vector<char> data(dataSize);
  result = RegQueryValueExA(hKey, valueName.c_str(), nullptr, nullptr, reinterpret_cast<BYTE *>(data.data()), &dataSize);
  if(result != ERROR_SUCCESS) {
    spdlog::error("Failed to query registry value. (Status={})", result);
    RegCloseKey(hKey);
    return {};
  }

  RegCloseKey(hKey);
  if(!data.empty() && data.back() == '\0')
    data.pop_back();
  return std::string{data.begin(), data.end()};
}

bool RegistryUtils::SetStringValue(HKEY hKeyParent, const std::string &subKey, const std::string &valueName, const std::string &newValue) {
  HKEY hKey;
  LONG result = RegOpenKeyExA(hKeyParent, subKey.c_str(), 0, KEY_SET_VALUE, &hKey);
  if(result != ERROR_SUCCESS) {
    spdlog::error("Failed to open registry key. (Status={})", result);
    return false;
  }
  result = RegSetValueExA(hKey, valueName.c_str(), 0, REG_SZ, (BYTE *)newValue.c_str(), newValue.size() + 1);
  if(result != ERROR_SUCCESS) {
    RegCloseKey(hKey);
    spdlog::error("Failed to set registry value. (Status={})", result);
    return false;
  }
  RegCloseKey(hKey);
  return true;
}

bool RegistryUtils::CreateKey(HKEY hKeyParent, const std::string &subKey) {
  HKEY hKey;
  LONG result = RegCreateKeyExA(hKeyParent, subKey.c_str(), 0, nullptr, 0, KEY_CREATE_SUB_KEY, nullptr, &hKey, nullptr);
  if(result != ERROR_SUCCESS) {
    spdlog::error("Failed to create registry key. (Status={})", result);
    return false;
  }
  RegCloseKey(hKey);
  return true;
}
