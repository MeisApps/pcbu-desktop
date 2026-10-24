#ifndef PCBU_DESKTOP_STARTUPCHECKS_H
#define PCBU_DESKTOP_STARTUPCHECKS_H

#include <optional>
#include <string>

class StartupChecks {
public:
  static std::optional<std::string> Run();
  static bool NeedsReinstall();

private:
  StartupChecks() = default;
};

#endif // PCBU_DESKTOP_STARTUPCHECKS_H
