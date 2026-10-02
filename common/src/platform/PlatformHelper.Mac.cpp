#include "PlatformHelper.h"

#include <climits>
#include <pwd.h>
#include <unistd.h>

#include <CoreServices/CoreServices.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "shell/LocalShell.h"
#include "utils/StringUtils.h"

std::vector<std::string> PlatformHelper::GetAllUsers() {
  std::vector<std::string> result{};
  auto cmdResult = LocalShell::RunUserCommand("dscl localhost -list /Local/Default/Users");
  for(const auto &userLine : StringUtils::Split(cmdResult.output, "\n")) {
    if(userLine.empty() || userLine.starts_with("_") || userLine == "daemon" || userLine == "nobody")
      continue;
    result.emplace_back(userLine);
  }
  return result;
}

std::string PlatformHelper::GetCurrentUser() {
  auto userName = SCDynamicStoreCopyConsoleUser(nullptr, nullptr, nullptr);
  if(!userName) {
    spdlog::error("Failed to find current user.");
    return {};
  }

  CFIndex bufferSize = CFStringGetLength(userName) + 1;
  char buffer[bufferSize];
  if(!CFStringGetCString(userName, buffer, bufferSize, kCFStringEncodingUTF8)) {
    spdlog::error("Failed to copy current user.");
    return {};
  }
  return {buffer};
}

bool PlatformHelper::HasUserPassword(const std::string &userName) {
  return true;
}

PlatformLoginStatus PlatformHelper::CheckLogin(const std::string &userName, const std::string &password) {
  bool hasUser = false;
  bool isValid = false;
  auto cfUsername = CFStringCreateWithCString(nullptr, userName.c_str(), kCFStringEncodingUTF8);
  auto cfPassword = CFStringCreateWithCString(nullptr, password.c_str(), kCFStringEncodingUTF8);
  auto query = CSIdentityQueryCreateForName(kCFAllocatorDefault, cfUsername, kCSIdentityQueryStringEquals, kCSIdentityClassUser,
                                            CSGetDefaultIdentityAuthority());
  CSIdentityQueryExecute(query, kCSIdentityQueryGenerateUpdateEvents, nullptr);
  auto idArray = CSIdentityQueryCopyResults(query);
  if(CFArrayGetCount(idArray) == 1) {
    hasUser = true;
    auto result = (CSIdentityRef)CFArrayGetValueAtIndex(idArray, 0);
    if(CSIdentityAuthenticateUsingPassword(result, cfPassword)) {
      isValid = true;
    }
  }

  CFRelease(cfUsername);
  CFRelease(cfPassword);
  CFRelease(idArray);
  CFRelease(query);
  if(!hasUser)
    return {PlatformLoginResult::INVALID_USER};
  if(!isValid)
    return {PlatformLoginResult::INVALID_PASSWORD};
  return {PlatformLoginResult::SUCCESS};
}

bool PlatformHelper::HasNativeLibrary(const std::string &libName) {
  return false;
}

std::filesystem::path PlatformHelper::GetUserHomeDir(const std::string &userName) {
  auto userStruct = userName.empty() ? getpwuid(geteuid()) : getpwnam(userName.c_str());
  if(!userStruct || !userStruct->pw_dir || userStruct->pw_dir[0] == '\0')
    return {};
  return userStruct->pw_dir;
}

std::filesystem::path PlatformHelper::GetUserDataDir() {
  auto homeDir = GetUserHomeDir();
  if(homeDir.empty())
    return {};
  return homeDir / "Library/Application Support";
}

std::filesystem::path PlatformHelper::GetUserLogsDir() {
  auto homeDir = GetUserHomeDir();
  if(homeDir.empty())
    return {};
  return homeDir / "Library/Logs";
}

std::filesystem::path PlatformHelper::GetSystemDataDir() {
  return "/etc";
}

std::filesystem::path PlatformHelper::GetSystemLogsDir() {
  return "/Library/Logs";
}

std::filesystem::path PlatformHelper::GetTempDir() {
  char tmpDir[PATH_MAX]{};
  if(confstr(_CS_DARWIN_USER_TEMP_DIR, tmpDir, sizeof(tmpDir)) == 0)
    return "/tmp";
  return tmpDir;
}
