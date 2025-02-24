#include "PlatformHelper.h"

// clang-format off
#include <Windows.h>
#include <WtsApi32.h>
#include <LM.h>
#include <sddl.h>
#include <spdlog/fmt/xchar.h>
#include <spdlog/spdlog.h>
// clang-format on

#include "NetworkHelper.h"
#include "shell/Shell.h"
#include "utils/RegistryUtils.h"
#include "utils/StringUtils.h"

std::wstring PlatformHelper_ResolveMSAccount(const std::wstring &domainStr, const std::wstring &userNameStr) {
  auto qualifiedName = fmt::format(L"{}\\{}", domainStr, userNameStr);
  LPBYTE bufPtr = nullptr;
  NET_API_STATUS resultCode = NetUserGetInfo(nullptr, userNameStr.c_str(), 24, &bufPtr);
  if(resultCode == NERR_Success) {
    auto pInfo = (LPUSER_INFO_24)bufPtr;
    if(pInfo != nullptr) {
      std::wstring internetProviderName = pInfo->usri24_internet_provider_name ? pInfo->usri24_internet_provider_name : L"";
      std::wstring internetPrincipalName = pInfo->usri24_internet_principal_name ? pInfo->usri24_internet_principal_name : L"";
      if(!internetProviderName.empty() && !internetPrincipalName.empty())
        qualifiedName = fmt::format(L"{}\\{}", internetProviderName, internetPrincipalName);
    }
  }
  if(bufPtr != nullptr)
    NetApiBufferFree(bufPtr);
  return qualifiedName;
}

std::string PlatformHelper_GetSidFromUserName(const std::string &userName) {
  auto accountUserName = StringUtils::ToWideString(userName);
  if(userName.starts_with("MicrosoftAccount\\")) {
    LPUSER_INFO_1 pBuf = nullptr;
    DWORD dwEntriesRead = 0;
    DWORD dwTotalEntries = 0;
    NET_API_STATUS nStatus = NetUserEnum(nullptr, 1, 0, (LPBYTE *)&pBuf, MAX_PREFERRED_LENGTH, &dwEntriesRead, &dwTotalEntries, nullptr);
    if(nStatus != NERR_Success) {
      spdlog::error("Failed to get computer users.");
      return {};
    }
    LPUSER_INFO_1 pTmpBuf = pBuf;
    if(pTmpBuf != nullptr) {
      auto computerName = StringUtils::ToWideString(NetworkHelper::GetHostName());
      for(DWORD i = 0; i < dwEntriesRead; i++) {
        if(pTmpBuf == nullptr)
          break;
        if((pTmpBuf->usri1_flags & UF_ACCOUNTDISABLE) == UF_ACCOUNTDISABLE || (pTmpBuf->usri1_flags & UF_LOCKOUT) == UF_LOCKOUT ||
           (pTmpBuf->usri1_flags & UF_NORMAL_ACCOUNT) != UF_NORMAL_ACCOUNT) {
          pTmpBuf++;
          continue;
        }
        auto nameStr = std::wstring(pTmpBuf->usri1_name);
        if(PlatformHelper_ResolveMSAccount(computerName, nameStr) == accountUserName) {
          accountUserName = fmt::format(L"{}\\{}", computerName, nameStr);
          break;
        }
        pTmpBuf++;
      }
    }
    if(pBuf != nullptr)
      NetApiBufferFree(pBuf);
  }

  SID_NAME_USE sidType;
  DWORD sidSize = 0;
  DWORD domainNameSize = 0;
  LookupAccountNameW(nullptr, accountUserName.c_str(), nullptr, &sidSize, nullptr, &domainNameSize, &sidType);
  if(GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    spdlog::error("Failed to get buffer sizes. (Code={})", GetLastError());
    return {};
  }
  auto pSid = static_cast<PSID>(malloc(sidSize));
  if(!pSid) {
    spdlog::error("Failed to allocate memory for SID.");
    return {};
  }
  std::wstring domainName(domainNameSize, '\0');
  if(!LookupAccountNameW(nullptr, accountUserName.c_str(), pSid, &sidSize, &domainName[0], &domainNameSize, &sidType)) {
    free(pSid);
    spdlog::error("Failed to look up account name. (Code={})", GetLastError());
    return {};
  }
  LPWSTR sidString = nullptr;
  if(!ConvertSidToStringSidW(pSid, &sidString)) {
    free(pSid);
    spdlog::error("Failed to convert SID to string. (Code={})", GetLastError());
    return {};
  }
  auto sidResult = StringUtils::FromWideString(sidString);
  free(pSid);
  LocalFree(sidString);
  return sidResult;
}

struct WinUserName {
  std::wstring domain{};
  std::wstring userName{};
};

std::optional<WinUserName> PlatformHelper_SplitUserName(const std::string &userName) {
  auto split = StringUtils::Split(userName, "\\");
  if(split.size() != 2)
    return {};
  WinUserName winUser{};
  winUser.domain = StringUtils::ToWideString(split[0]);
  winUser.userName = StringUtils::ToWideString(split[1]);
  return winUser;
}

std::string PlatformHelper::GetDeviceUUID() {
  auto uuidResult = Shell::RunCommand("wmic csproduct get uuid");
  if(uuidResult.exitCode == 0 && uuidResult.output.size() > 4)
    return StringUtils::Trim(uuidResult.output.substr(4));
  auto regUuid = RegistryUtils::GetStringValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", "MachineGuid");
  if(regUuid.has_value() && !regUuid.value().empty())
    return regUuid.value();
  spdlog::error("Failed to find device UUID.");
  return {};
}

bool PlatformHelper::HasNativeLibrary(const std::string &libName) {
  return false;
}

std::vector<std::string> PlatformHelper::GetAllUsers() {
  std::vector<std::string> result{};
  LPUSER_INFO_1 pBuf = nullptr;
  DWORD dwEntriesRead = 0;
  DWORD dwTotalEntries = 0;
  NET_API_STATUS nStatus = NetUserEnum(nullptr, 1, 0, (LPBYTE *)&pBuf, MAX_PREFERRED_LENGTH, &dwEntriesRead, &dwTotalEntries, nullptr);
  if(nStatus != NERR_Success) {
    spdlog::error("Failed to get computer users.");
    return result;
  }
  LPUSER_INFO_1 pTmpBuf = pBuf;
  if(pTmpBuf != nullptr) {
    auto computerName = StringUtils::ToWideString(NetworkHelper::GetHostName());
    for(DWORD i = 0; i < dwEntriesRead; i++) {
      if(pTmpBuf == nullptr)
        break;
      if((pTmpBuf->usri1_flags & UF_ACCOUNTDISABLE) == UF_ACCOUNTDISABLE || (pTmpBuf->usri1_flags & UF_LOCKOUT) == UF_LOCKOUT ||
         (pTmpBuf->usri1_flags & UF_NORMAL_ACCOUNT) != UF_NORMAL_ACCOUNT) {
        pTmpBuf++;
        continue;
      }
      result.emplace_back(StringUtils::FromWideString(PlatformHelper_ResolveMSAccount(computerName, pTmpBuf->usri1_name)));
      pTmpBuf++;
    }
  }
  if(pBuf != nullptr)
    NetApiBufferFree(pBuf);
  return result;
}

std::string PlatformHelper::GetCurrentUser() {
  DWORD sessionId = WTSGetActiveConsoleSessionId();
  if(sessionId == 0xFFFFFFFF) {
    spdlog::error("Failed to get user session.");
    return {};
  }
  LPWSTR userName = nullptr;
  DWORD userNameSize = 0;
  if(!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionId, WTSUserName, &userName, &userNameSize)) {
    spdlog::error("Failed to get user name. (Code={})", GetLastError());
    return {};
  }
  LPWSTR domainName = nullptr;
  DWORD domainNameSize = 0;
  if(!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionId, WTSDomainName, &domainName, &domainNameSize)) {
    WTSFreeMemory(userName);
    spdlog::error("Failed to get user domain. (Code={})", GetLastError());
    return {};
  }
  WTSFreeMemory(userName);
  WTSFreeMemory(domainName);
  return StringUtils::FromWideString(PlatformHelper_ResolveMSAccount(domainName, userName));
}

bool PlatformHelper::HasUserPassword(const std::string &userName) {
  auto winUser = PlatformHelper_SplitUserName(userName);
  if(!winUser.has_value())
    return false;
  HANDLE hToken = nullptr;
  LogonUserW(winUser.value().userName.c_str(), winUser.value().domain.c_str(), L"", LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &hToken);
  DWORD error = GetLastError();
  if(hToken != nullptr)
    CloseHandle(hToken);
  return error != 1327;
}

PlatformLoginResult PlatformHelper::CheckLogin(const std::string &userName, const std::string &password) {
  auto winUser = PlatformHelper_SplitUserName(userName);
  if(!winUser.has_value())
    return PlatformLoginResult::INVALID_USER;
  HANDLE hToken = nullptr;
  BOOL result = LogonUserW(winUser.value().userName.c_str(), winUser.value().domain.c_str(), StringUtils::ToWideString(password).c_str(),
                           LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &hToken);
  DWORD error = GetLastError();
  if(hToken != nullptr)
    CloseHandle(hToken);
  if(result)
    return PlatformLoginResult::SUCCESS;
  spdlog::warn("LogonUserW() failed. (Code={})", error);
  if(error == ERROR_ACCOUNT_LOCKED_OUT)
    return PlatformLoginResult::ACCOUNT_LOCKED;
  return PlatformLoginResult::INVALID_PASSWORD;
}

bool PlatformHelper::SetDefaultCredProv(const std::string &userName, const std::string &provId) {
  auto userSid = PlatformHelper_GetSidFromUserName(userName);
  if(userSid.empty())
    return false;
  return RegistryUtils::SetStringValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\LogonUI\UserTile)", userSid,
                                       provId);
}
