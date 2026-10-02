#ifndef PCBU_DESKTOP_PLATFORMHELPER_H
#define PCBU_DESKTOP_PLATFORMHELPER_H

#include <cstdint>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

enum class PlatformLoginResult {
  SUCCESS,
  INVALID_USER,
  INVALID_PASSWORD,
  ACCOUNT_LOCKED,
  ACCOUNT_RESTRICTED,
  PW_EXPIRED,
  ACCOUNT_DISABLED,
  LOGON_TYPE_DENIED,
  UNKNOWN_ERROR
};

struct PlatformLoginStatus {
  PlatformLoginResult result{PlatformLoginResult::UNKNOWN_ERROR};
  uint32_t errorCode{};
};

class PlatformHelper {
public:
  static bool HasNativeLibrary(const std::string &libName);

  static std::vector<std::string> GetAllUsers();
  static std::string GetCurrentUser();
  static bool HasUserPassword(const std::string &userName);

  static PlatformLoginStatus CheckLogin(const std::string &userName, const std::string &password);

  static std::filesystem::path GetUserHomeDir(const std::string &userName = {});
  static std::filesystem::path GetUserDataDir();
  static std::filesystem::path GetUserLogsDir();
  static std::filesystem::path GetSystemDataDir();
  static std::filesystem::path GetSystemLogsDir();
  static std::filesystem::path GetTempDir();
#ifdef WINDOWS
  static std::filesystem::path GetProgramFilesDir();
#endif

#ifdef WINDOWS
  static bool SetDefaultCredProv(const std::string &userName, const std::string &provId);
#endif
private:
  PlatformHelper() = default;

#ifdef WINDOWS
  static std::filesystem::path GetShellFolder(int csidl);
#endif
};

#endif // PCBU_DESKTOP_PLATFORMHELPER_H
