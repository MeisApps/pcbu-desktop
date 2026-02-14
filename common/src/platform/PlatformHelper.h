#ifndef PCBU_DESKTOP_PLATFORMHELPER_H
#define PCBU_DESKTOP_PLATFORMHELPER_H

#include <spdlog/spdlog.h>
#include <string>
#include <vector>

enum class PlatformLoginResult { SUCCESS, INVALID_USER, INVALID_PASSWORD, ACCOUNT_LOCKED };

class PlatformHelper {
public:
  static bool HasNativeLibrary(const std::string &libName);

  static std::vector<std::string> GetAllUsers();
  static std::string GetCurrentUser();
  static bool HasUserPassword(const std::string &userName);

  static PlatformLoginResult CheckLogin(const std::string &userName, const std::string &password);

#ifdef WINDOWS
  static bool SetDefaultCredProv(const std::string &userName, const std::string &provId);
#endif
private:
  PlatformHelper() = default;
};

#endif // PCBU_DESKTOP_PLATFORMHELPER_H
