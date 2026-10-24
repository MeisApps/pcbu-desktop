#ifndef PCBU_DESKTOP_SSHSERVERHELPER_H
#define PCBU_DESKTOP_SSHSERVERHELPER_H

#include <atomic>
#include <string>

#include "handler/UnlockHandler.h"
#include "storage/PairedDevicesStorage.h"

class SshServerHelper {
public:
  static std::string AddServer(UnlockHandler &unlockHandler, const PairedDevice &device, const SshServer &server, const std::string &password,
                               std::atomic<bool> *isRunning);
  static bool RemoveServer(const std::string &deviceId, int serverIndex);

private:
  SshServerHelper() = default;
};

#endif // PCBU_DESKTOP_SSHSERVERHELPER_H
