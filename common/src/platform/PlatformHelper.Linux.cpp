#include "PlatformHelper.h"

#include <crypt.h>
#include <cstring>
#include <optional>
#include <pwd.h>
#include <shadow.h>
#include <unistd.h>

#include "shell/Shell.h"
#include "utils/StringUtils.h"

std::vector<std::string> PlatformHelper::GetAllUsers() {
  auto data = Shell::ReadBytes("/etc/passwd");
  auto passwdStr = std::string(data.begin(), data.end());
  std::vector<std::string> result{};
  for(const auto &entry : StringUtils::Split(passwdStr, "\n")) {
    auto split = StringUtils::Split(entry, ":");
    if(split.size() < 3)
      continue;
    char *end{};
    auto uidStr = split[2].c_str();
    auto uid = std::strtol(uidStr, &end, 10);
    if(end == uidStr || (uid != 0 && (uid < 1000 || uid > 60000)))
      continue;
    auto userName = split[0];
    result.push_back(userName);
  }
  return result;
}

std::string PlatformHelper::GetCurrentUser() {
  std::optional<uid_t> uid{};
  auto sudoUidStr = std::getenv("SUDO_UID");
  if(sudoUidStr != nullptr) {
    char *end{};
    auto sudoUid = std::strtol(sudoUidStr, &end, 10);
    if(end != sudoUidStr)
      uid = sudoUid;
  }
  auto pkExecUidStr = std::getenv("PKEXEC_UID");
  if(pkExecUidStr != nullptr) {
    char *end{};
    auto pkExecUid = std::strtol(pkExecUidStr, &end, 10);
    if(end != pkExecUidStr)
      uid = pkExecUid;
  }

  if(!uid.has_value())
    uid = getuid();
  auto userStruct = getpwuid(uid.value());
  if(userStruct == nullptr) {
    spdlog::error("Failed to find current user.");
    return {};
  }
  return userStruct->pw_name;
}

bool PlatformHelper::HasUserPassword(const std::string &userName) {
  return true;
}

PlatformLoginStatus PlatformHelper::CheckLogin(const std::string &userName, const std::string &password) {
  struct passwd *passwdEntry = getpwnam(userName.c_str());
  if(!passwdEntry) {
    spdlog::error("Failed to read passwd entry for user {}.", userName);
    return {PlatformLoginResult::INVALID_USER};
  }
  if(strcmp(passwdEntry->pw_passwd, "x") != 0) {
    if(strcmp(passwdEntry->pw_passwd, crypt(password.c_str(), passwdEntry->pw_passwd)) == 0)
      return {PlatformLoginResult::SUCCESS};
    return {PlatformLoginResult::INVALID_PASSWORD};
  }

  struct spwd *shadowEntry = getspnam(userName.c_str());
  if(!shadowEntry) {
    spdlog::error("Failed to read shadow entry for user {}." + userName);
    return {PlatformLoginResult::INVALID_USER};
  }
  if(strcmp(shadowEntry->sp_pwdp, crypt(password.c_str(), shadowEntry->sp_pwdp)) == 0)
    return {PlatformLoginResult::SUCCESS};
  return {PlatformLoginResult::INVALID_PASSWORD};
}

bool PlatformHelper::HasNativeLibrary(const std::string &libName) {
  return std::filesystem::exists(std::filesystem::path("/lib64") / libName) || std::filesystem::exists(std::filesystem::path("/lib") / libName) ||
         std::filesystem::exists(std::filesystem::path("/usr/lib/x86_64-linux-gnu") / libName) ||
         std::filesystem::exists(std::filesystem::path("/usr/lib/aarch64-linux-gnu") / libName);
}

std::filesystem::path PlatformHelper::GetUserHomeDir(const std::string &userName) {
  auto userStruct = userName.empty() ? getpwuid(geteuid()) : getpwnam(userName.c_str());
  if(!userStruct || !userStruct->pw_dir || userStruct->pw_dir[0] == '\0')
    return {};
  return userStruct->pw_dir;
}

std::filesystem::path PlatformHelper::GetUserDataDir() {
  auto dataHome = std::getenv("XDG_DATA_HOME");
  if(dataHome && dataHome[0] != '\0')
    return dataHome;
  auto homeDir = GetUserHomeDir();
  if(homeDir.empty())
    return {};
  return homeDir / ".local/share";
}

std::filesystem::path PlatformHelper::GetUserLogsDir() {
  auto stateHome = std::getenv("XDG_STATE_HOME");
  if(stateHome && stateHome[0] != '\0')
    return stateHome;
  auto homeDir = GetUserHomeDir();
  if(homeDir.empty())
    return {};
  return homeDir / ".local/state";
}

std::filesystem::path PlatformHelper::GetSystemDataDir() {
  return "/etc";
}

std::filesystem::path PlatformHelper::GetSystemLogsDir() {
  return "/var/log";
}

std::filesystem::path PlatformHelper::GetTempDir() {
  auto tmpDir = std::getenv("TMPDIR");
  if(tmpDir && tmpDir[0] != '\0')
    return tmpDir;
  return "/tmp";
}
