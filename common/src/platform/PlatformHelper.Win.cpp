#include "PlatformHelper.h"

#include <Windows.h>
#include <LM.h>
#include <WtsApi32.h>
#include <sddl.h>
#include <spdlog/spdlog.h>

#include "NetworkHelper.h"
#include "shell/Shell.h"
#include "utils/RegistryUtils.h"
#include "utils/StringUtils.h"

std::string ResolveMSAccount(const std::string& domainStr, const std::string& userNameStr) {
    std::string qualifiedName = fmt::format("{}\\{}", domainStr, userNameStr);
    LPBYTE bufPtr = nullptr;
    NET_API_STATUS resultCode = NetUserGetInfo(nullptr, StringUtils::ToWideString(userNameStr).c_str(), 24, &bufPtr);
    if (resultCode == NERR_Success) {
        auto pInfo = (LPUSER_INFO_24)bufPtr;
        if (pInfo != nullptr) {
            std::wstring internetProviderName = pInfo->usri24_internet_provider_name ? pInfo->usri24_internet_provider_name : L"";
            std::wstring internetPrincipalName = pInfo->usri24_internet_principal_name ? pInfo->usri24_internet_principal_name : L"";
            if (!internetProviderName.empty() && !internetPrincipalName.empty())
                qualifiedName = fmt::format("{}\\{}", StringUtils::FromWideString(internetProviderName), StringUtils::FromWideString(internetPrincipalName));
        }
    }
    if (bufPtr != nullptr)
        NetApiBufferFree(bufPtr);
    return qualifiedName;
}

std::string GetSidFromUserName(const std::string &userName) {
    auto accountUserName = userName;
    if(userName.starts_with("MicrosoftAccount\\")) {
        LPUSER_INFO_1 pBuf = nullptr;
        DWORD dwEntriesRead = 0;
        DWORD dwTotalEntries = 0;
        NET_API_STATUS nStatus = NetUserEnum(nullptr, 1, 0, (LPBYTE*)&pBuf, MAX_PREFERRED_LENGTH, &dwEntriesRead, &dwTotalEntries, nullptr);
        if (nStatus != NERR_Success) {
            spdlog::error("Failed to get computer users.");
            return {};
        }
        LPUSER_INFO_1 pTmpBuf = pBuf;
        if (pTmpBuf != nullptr) {
            auto computerName = NetworkHelper::GetHostName();
            for (DWORD i = 0; i < dwEntriesRead; i++) {
                if (pTmpBuf == nullptr)
                    break;
                if ((pTmpBuf->usri1_flags & UF_ACCOUNTDISABLE) == UF_ACCOUNTDISABLE ||
                    (pTmpBuf->usri1_flags & UF_LOCKOUT) == UF_LOCKOUT ||
                    (pTmpBuf->usri1_flags & UF_NORMAL_ACCOUNT) != UF_NORMAL_ACCOUNT) {
                    pTmpBuf++;
                    continue;
                }
                auto nameStr = StringUtils::FromWideString(pTmpBuf->usri1_name);
                if(ResolveMSAccount(computerName, nameStr) == userName) {
                    accountUserName = fmt::format("{}\\{}", computerName, nameStr);
                    break;
                }
                pTmpBuf++;
            }
        }
        if (pBuf != nullptr)
            NetApiBufferFree(pBuf);
    }

    SID_NAME_USE sidType;
    DWORD sidSize = 0;
    DWORD domainNameSize = 0;
    LookupAccountNameA(nullptr, accountUserName.c_str(), nullptr, &sidSize,
                       nullptr, &domainNameSize, &sidType);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        spdlog::error("Failed to get buffer sizes. (Code={})", GetLastError());
        return {};
    }
    auto pSid = static_cast<PSID>(malloc(sidSize));
    if (!pSid) {
        spdlog::error("Failed to allocate memory for SID.");
        return {};
    }
    std::string domainName(domainNameSize, '\0');
    if (!LookupAccountNameA(nullptr, accountUserName.c_str(), pSid, &sidSize,
                            &domainName[0], &domainNameSize, &sidType)) {
        free(pSid);
        spdlog::error("Failed to look up account name. (Code={})", GetLastError());
        return {};
    }
    LPSTR sidString = nullptr;
    if (!ConvertSidToStringSidA(pSid, &sidString)) {
        free(pSid);
        spdlog::error("Failed to convert SID to string. (Code={})", GetLastError());
        return {};
    }
    std::string sidResult(sidString);
    free(pSid);
    LocalFree(sidString);
    return sidResult;
}

std::string PlatformHelper::GetDeviceUUID() {
    auto uuidResult = Shell::RunCommand("wmic csproduct get uuid");
    if(uuidResult.exitCode == 0 && uuidResult.output.size() > 4)
        return StringUtils::Trim(uuidResult.output.substr(4));
    auto regUuid = RegistryUtils::GetStringValue(HKEY_LOCAL_MACHINE,
                                                 "SOFTWARE\\Microsoft\\Cryptography",
                                                 "MachineGuid");
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
    NET_API_STATUS nStatus = NetUserEnum(nullptr, 1, 0, (LPBYTE*)&pBuf, MAX_PREFERRED_LENGTH, &dwEntriesRead, &dwTotalEntries, nullptr);
    if (nStatus != NERR_Success) {
        spdlog::error("Failed to get computer users.");
        return result;
    }
    LPUSER_INFO_1 pTmpBuf = pBuf;
    if (pTmpBuf != nullptr) {
        auto computerName = NetworkHelper::GetHostName();
        for (DWORD i = 0; i < dwEntriesRead; i++) {
            if (pTmpBuf == nullptr)
                break;
            if ((pTmpBuf->usri1_flags & UF_ACCOUNTDISABLE) == UF_ACCOUNTDISABLE ||
                (pTmpBuf->usri1_flags & UF_LOCKOUT) == UF_LOCKOUT ||
                (pTmpBuf->usri1_flags & UF_NORMAL_ACCOUNT) != UF_NORMAL_ACCOUNT) {
                pTmpBuf++;
                continue;
            }
            result.emplace_back(ResolveMSAccount(computerName, StringUtils::FromWideString(pTmpBuf->usri1_name)));
            pTmpBuf++;
        }
    }
    if (pBuf != nullptr)
        NetApiBufferFree(pBuf);
    return result;
}

std::string PlatformHelper::GetCurrentUser() {
    DWORD sessionId = WTSGetActiveConsoleSessionId();
    if (sessionId == 0xFFFFFFFF) {
        spdlog::error("Failed to get user session.");
        return {};
    }
    LPSTR userName = nullptr;
    DWORD userNameSize = 0;
    if (!WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, sessionId, WTSUserName, &userName, &userNameSize)) {
        spdlog::error("Failed to get user name. (Code={})", GetLastError());
        return {};
    }
    LPSTR domainName = nullptr;
    DWORD domainNameSize = 0;
    if (!WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, sessionId, WTSDomainName, &domainName, &domainNameSize)) {
        WTSFreeMemory(userName);
        spdlog::error("Failed to get user domain. (Code={})", GetLastError());
        return {};
    }
    WTSFreeMemory(userName);
    WTSFreeMemory(domainName);
    return ResolveMSAccount(domainName, userName);
}

bool PlatformHelper::HasUserPassword(const std::string &userName) {
    auto split = StringUtils::Split(userName, "\\");
    if(split.size() != 2)
        return false;
    HANDLE hToken = nullptr;
    LogonUserA(split[1].c_str(), split[0].c_str(), "",
               LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &hToken);
    DWORD error = GetLastError();
    if (hToken != nullptr) CloseHandle(hToken);
    return error != 1327;
}

PlatformLoginResult PlatformHelper::CheckLogin(const std::string &userName, const std::string &password) {
    auto split = StringUtils::Split(userName, "\\");
    if(split.size() != 2)
        return PlatformLoginResult::INVALID_USER;
    HANDLE hToken = nullptr;
    BOOL result = LogonUserA(split[1].c_str(), split[0].c_str(), password.c_str(),
                             LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &hToken);
    DWORD error = GetLastError();
    if (hToken != nullptr) CloseHandle(hToken);
    if(result)
        return PlatformLoginResult::SUCCESS;
    spdlog::warn("LogonUserA() failed. (Code={})", error);
    if(error == ERROR_ACCOUNT_LOCKED_OUT)
        return PlatformLoginResult::ACCOUNT_LOCKED;
    return PlatformLoginResult::INVALID_PASSWORD;
}

bool PlatformHelper::SetDefaultCredProv(const std::string &userName, const std::string &provId) {
    auto userSid = GetSidFromUserName(userName);
    if(userSid.empty())
        return false;
    return RegistryUtils::SetStringValue(HKEY_LOCAL_MACHINE,
                                         R"(SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\LogonUI\UserTile)",
                                         userSid, provId);
}
