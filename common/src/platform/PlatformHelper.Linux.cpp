#include "PlatformHelper.h"

#include <cstring>
#include <optional>
#include <unistd.h>
#include <pwd.h>
#include <shadow.h>

#include "shell/Shell.h"
#include "utils/StringUtils.h"

std::string PlatformHelper::GetDeviceUUID() {
    auto data = Shell::ReadBytes("/etc/machine-id");
    std::string result = {data.begin(), data.end()};
    if(result.empty())
        spdlog::error("Failed to find device UUID.");
    return result;
}

std::vector<std::string> PlatformHelper::GetAllUsers() {
    auto data = Shell::ReadBytes("/etc/passwd");
    auto passwdStr = std::string(data.begin(), data.end());
    std::vector<std::string> result{};
    for(const auto& entry : StringUtils::Split(passwdStr, "\n")) {
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

bool PlatformHelper::CheckLogin(const std::string &userName, const std::string &password) {
    struct passwd *passwdEntry = getpwnam(userName.c_str());
    if (!passwdEntry) {
        spdlog::error("Failed to read passwd entry for user {}.", userName);
        return false;
    }
    if (strcmp(passwdEntry->pw_passwd, "x") != 0)
        return strcmp(passwdEntry->pw_passwd, crypt(password.c_str(), passwdEntry->pw_passwd)) == 0;

    struct spwd *shadowEntry = getspnam(userName.c_str());
    if (!shadowEntry) {
        spdlog::error("Failed to read shadow entry for user {}." + userName);
        return false;
    }
    return strcmp(shadowEntry->sp_pwdp, crypt(password.c_str(), shadowEntry->sp_pwdp)) == 0;
}

bool PlatformHelper::HasNativeLibrary(const std::string &libName) {
    return std::filesystem::exists(std::filesystem::path("/lib64") / libName) ||
            std::filesystem::exists(std::filesystem::path("/lib") / libName) ||
            std::filesystem::exists(std::filesystem::path("/usr/lib/x86_64-linux-gnu") / libName) ||
            std::filesystem::exists(std::filesystem::path("/usr/lib/aarch64-linux-gnu") / libName);
}
