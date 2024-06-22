#ifndef PCBU_DESKTOP_WINFIREWALLHELPER_H
#define PCBU_DESKTOP_WINFIREWALLHELPER_H

#include <string>

class WinFirewallHelper {
public:
    static bool RemoveAllRulesForProgram(const std::string& exePath);

private:
    WinFirewallHelper() = default;
};


#endif //PCBU_DESKTOP_WINFIREWALLHELPER_H
