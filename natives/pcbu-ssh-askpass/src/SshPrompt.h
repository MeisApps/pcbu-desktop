#ifndef PCBU_DESKTOP_SSHPROMPT_H
#define PCBU_DESKTOP_SSHPROMPT_H

#include <optional>
#include <string>

#include "storage/PairedDevicesStorage.h"

struct SshPrompt {
  enum class Kind { Unknown, Password, Key };
  Kind kind{Kind::Unknown};
  std::string user{};
  std::string host{};
  std::string keyPath{};

  [[nodiscard]] std::string GetDisplayName() const;
  [[nodiscard]] std::optional<SshServer> FindMatch(const PairedDevice &device, const std::string &homeDir) const;

  static SshPrompt Parse(const std::string &raw);
};

#endif // PCBU_DESKTOP_SSHPROMPT_H
