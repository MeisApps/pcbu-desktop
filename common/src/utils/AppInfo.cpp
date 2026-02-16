#include "AppInfo.h"

#include "StringUtils.h"
#include "generated/commit.h"

#include <spdlog/spdlog.h>

std::string AppInfo::GetVersion() {
  std::string version = "3.0.2";
  if(PCBU_DEBUG)
    version += fmt::format("-{}-{}", GIT_BRANCH, GIT_COMMIT_HASH);
  return version;
}

std::string AppInfo::GetProtocolVersion() {
  return "2.0.0";
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
    auto thisStr = StringUtils::Split(thisVersion, "-")[0];
    auto otherStr = StringUtils::Split(otherVersion, "-")[0];
    auto thisParts = StringUtils::Split(thisStr, ".");
    auto otherParts = StringUtils::Split(otherStr, ".");
    std::vector<int> thisVersions{}, otherVersions{};
    for(const auto &part : thisParts)
      thisVersions.push_back(std::stoi(part));
    for(const auto &part : otherParts)
      otherVersions.push_back(std::stoi(part));
    for(size_t i = 0; i < std::max(thisVersions.size(), otherVersions.size()); ++i) {
      int thisNum = (i < thisVersions.size()) ? thisVersions[i] : 0;
      int otherNum = (i < otherVersions.size()) ? otherVersions[i] : 0;
      if(thisNum < otherNum)
        return 1;
      if(thisNum > otherNum)
        return -1;
    }
    return 0;
  } catch(...) {
  }
  return 0;
}
