#ifndef PCBU_CLI_PAIRINGCOMMAND_H
#define PCBU_CLI_PAIRINGCOMMAND_H

#include <string>
#include <vector>

#include "cli/CommandLine.h"
#include "storage/PairingMethod.h"

class PairingCommand {
public:
  static int Run(const std::vector<std::string> &args);

private:
  PairingCommand() = default;

  static std::string SelectUser(const CommandLine &cmd, bool &isManualUserName);
  static std::string ReadPassword(const CommandLine &cmd, const std::string &userName);
  static PairingMethod SelectMethod(const CommandLine &cmd);
  static std::string SelectBluetoothDevice(const CommandLine &cmd);
};

#endif // PCBU_CLI_PAIRINGCOMMAND_H
