#ifndef PCBU_DESKTOP_PLATFORMHELPER_H
#define PCBU_DESKTOP_PLATFORMHELPER_H

#include <string>
#include <vector>
#include <spdlog/spdlog.h>

class PlatformHelper {
public:
    static std::string GetDeviceUUID();
    static bool HasNativeLibrary(const std::string& libName);

    static std::vector<std::string> GetAllUsers();
    static std::string GetCurrentUser();
    static bool HasUserPassword(const std::string& userName);

    static bool CheckLogin(const std::string& userName, const std::string& password);

#ifdef WINDOWS
    static bool SetDefaultCredProv(const std::string& userName, const std::string& provId);
#endif
private:
    PlatformHelper() = default;
};

#endif //PCBU_DESKTOP_PLATFORMHELPER_H
