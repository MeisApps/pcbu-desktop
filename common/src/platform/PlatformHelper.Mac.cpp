#include "PlatformHelper.h"

#include <SystemConfiguration/SystemConfiguration.h>
#include <CoreServices/CoreServices.h>

#include "shell/Shell.h"
#include "utils/StringUtils.h"

std::string PlatformHelper::GetDeviceUUID() {
    auto platformExpert = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    if (platformExpert) {
        auto uuid = (CFStringRef)IORegistryEntryCreateCFProperty(platformExpert, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
        IOObjectRelease(platformExpert);
        if (uuid) {
            auto uuidCStr = CFStringGetCStringPtr(uuid, kCFStringEncodingUTF8);
            std::string uuidStr = uuidCStr ? uuidCStr : "";
            CFRelease(uuid);
            return uuidStr;
        }
    }
    spdlog::error("Failed to find device UUID.");
    return {};
}

std::vector<std::string> PlatformHelper::GetAllUsers() {
    std::vector<std::string> result{};
    auto cmdResult = Shell::RunUserCommand("dscl localhost -list /Local/Default/Users");
    for(const auto& userLine : StringUtils::Split(cmdResult.output, "\n")) {
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
    if (!CFStringGetCString(userName, buffer, bufferSize, kCFStringEncodingUTF8)) {
        spdlog::error("Failed to copy current user.");
        return {};
    }
    return {buffer};
}

bool PlatformHelper::HasUserPassword(const std::string &userName) {
    return true;
}

PlatformLoginResult PlatformHelper::CheckLogin(const std::string &userName, const std::string &password) {
    bool hasUser = false;
    bool isValid = false;
    auto cfUsername = CFStringCreateWithCString(nullptr, userName.c_str(), kCFStringEncodingUTF8);
    auto cfPassword = CFStringCreateWithCString(nullptr, password.c_str(), kCFStringEncodingUTF8);
    auto query = CSIdentityQueryCreateForName(kCFAllocatorDefault, cfUsername, kCSIdentityQueryStringEquals, kCSIdentityClassUser, CSGetDefaultIdentityAuthority());
    CSIdentityQueryExecute(query, kCSIdentityQueryGenerateUpdateEvents, nullptr);
    auto idArray = CSIdentityQueryCopyResults(query);
    if (CFArrayGetCount(idArray) == 1) {
        hasUser = true;
        auto result = (CSIdentityRef) CFArrayGetValueAtIndex(idArray, 0);
        if (CSIdentityAuthenticateUsingPassword(result, cfPassword)) {
            isValid = true;
        }
    }

    CFRelease(cfUsername);
    CFRelease(cfPassword);
    CFRelease(idArray);
    CFRelease(query);
    if(!hasUser)
        return PlatformLoginResult::INVALID_USER;
    if(!isValid)
        return PlatformLoginResult::INVALID_PASSWORD;
    return PlatformLoginResult::SUCCESS;
}

bool PlatformHelper::HasNativeLibrary(const std::string &libName) {
    return false;
}
