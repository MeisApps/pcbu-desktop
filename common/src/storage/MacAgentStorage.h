#ifndef PCBU_DESKTOP_MACAGENTSTORAGE_H
#define PCBU_DESKTOP_MACAGENTSTORAGE_H

#include <optional>
#include <string_view>

class MacAgentStorage {
public:
  static std::optional<bool> HasAccessibility();
  static void SetAccessibility(bool hasAccessibility);

  static void RequestAccessibility();

  static constexpr std::string_view ACCESSIBILITY_REQUEST_NOTIFICATION = "com.meisapps.PulseUnlock.Agent.RequestAccessibility";

private:
  MacAgentStorage() = default;

  static constexpr std::string_view AGENT_FILE_NAME = "mac_agent.json";
};

#endif
