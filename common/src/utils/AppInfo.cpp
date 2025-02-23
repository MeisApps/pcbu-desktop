#include "AppInfo.h"

#include "StringUtils.h"

std::string AppInfo::GetVersion() {
    return "2.1.0";
}

std::string AppInfo::GetProtocolVersion() {
    return "1.5.0";
}

std::string AppInfo::GetOperatingSystem() {
#ifdef WINDOWS
    return "Windows";
#elif LINUX
    return "Linux";
#elif APPLE
    return "macOS";
#else
#error Unknown operating system.
#endif
}

std::string AppInfo::GetArchitecture() {
    return TARGET_ARCH;
}

int AppInfo::CompareVersion(const std::string &thisVersion, const std::string &otherVersion) {
    try {
        auto thisParts = StringUtils::Split(thisVersion, ".");
        auto otherParts = StringUtils::Split(otherVersion, ".");
        std::vector<int> thisVersions{}, otherVersions{};
        for (const auto& part : thisParts)
            thisVersions.push_back(std::stoi(part));
        for (const auto& part : otherParts)
            otherVersions.push_back(std::stoi(part));
        for (size_t i = 0; i < std::max(thisVersions.size(), otherVersions.size()); ++i) {
            int thisNum = (i < thisVersions.size()) ? thisVersions[i] : 0;
            int otherNum = (i < otherVersions.size()) ? otherVersions[i] : 0;
            if (thisNum < otherNum)
                return 1;
            if (thisNum > otherNum)
                return -1;
        }
        return 0;
    } catch(...) {}
    return 0;
}
