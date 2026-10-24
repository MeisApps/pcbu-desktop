#ifndef PCBU_CLI_DEVICECOMMANDS_H
#define PCBU_CLI_DEVICECOMMANDS_H

#include <string>
#include <vector>

#include "storage/PairedDevicesStorage.h"

class DeviceCommands {
public:
  static int Devices(const std::vector<std::string> &args);
  static int Ssh(const std::vector<std::string> &args);
  static int Test(const std::vector<std::string> &args);

private:
  DeviceCommands() = default;

  static PairedDevice FindDevice(const std::string &idOrPrefix);
  static void PrintSshServers(const PairedDevice &device);
};

#endif // PCBU_CLI_DEVICECOMMANDS_H
