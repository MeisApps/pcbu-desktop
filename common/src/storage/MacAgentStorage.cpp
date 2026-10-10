#include "MacAgentStorage.h"

#include <CoreFoundation/CoreFoundation.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "AppSettings.h"
#include "shell/LocalShell.h"

std::optional<bool> MacAgentStorage::HasAccessibility() {
  auto filePath = AppSettings::GetBaseUserDir() / AGENT_FILE_NAME;
  if(!std::filesystem::exists(filePath))
    return {};
  auto jsonData = LocalShell::ReadBytes(filePath);
  if(jsonData.empty())
    return {};

  try {
    auto json = nlohmann::json::parse(jsonData);
    return json.value("hasAccessibility", false);
  } catch(const std::exception &ex) {
    spdlog::error("Failed reading mac agent status: {}", ex.what());
    return {};
  }
}

void MacAgentStorage::SetAccessibility(bool hasAccessibility) {
  try {
    nlohmann::json json = {
        {"hasAccessibility", hasAccessibility},
    };
    auto filePath = AppSettings::GetBaseUserDir() / AGENT_FILE_NAME;
    if(!std::filesystem::exists(filePath.parent_path()))
      LocalShell::CreateDir(filePath.parent_path());
    auto jsonStr = json.dump();
    if(!LocalShell::WriteBytes(filePath, {jsonStr.begin(), jsonStr.end()}))
      spdlog::error("Failed writing mac agent status to '{}'.", filePath.string());
  } catch(const std::exception &ex) {
    spdlog::error("Failed writing mac agent status: {}", ex.what());
  }
}

void MacAgentStorage::RequestAccessibility() {
  auto name = CFStringCreateWithCString(nullptr, std::string(ACCESSIBILITY_REQUEST_NOTIFICATION).c_str(), kCFStringEncodingUTF8);
  CFNotificationCenterPostNotification(CFNotificationCenterGetDistributedCenter(), name, nullptr, nullptr, true);
  CFRelease(name);
}
