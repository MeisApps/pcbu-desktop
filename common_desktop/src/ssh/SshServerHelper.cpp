#include "SshServerHelper.h"

#include "utils/CryptUtils.h"
#include "utils/I18n.h"

std::string SshServerHelper::AddServer(UnlockHandler &unlockHandler, const PairedDevice &device, const SshServer &server,
                                       const std::string &password, std::atomic<bool> *isRunning) {
  auto result = unlockHandler.GetResult(device.userName, "Add server (SSH)", {device.id}, isRunning);
  if(result.state != UnlockState::SUCCESS)
    return UnlockStateUtils::ToString(result.state);
  if(result.device.id != device.id || result.passwordKey.empty())
    return I18n::Get("error_ssh_wrong_device");

  auto enc = CryptUtils::EncryptAES(password, result.passwordKey);
  if(!enc.has_value())
    return I18n::Get("error_ssh_encrypt");
  auto newDevice = PairedDevicesStorage::GetDeviceByID(device.id);
  if(!newDevice.has_value())
    return I18n::Get("error_unknown");

  auto newServer = server;
  newServer.passwordEnc = enc.value();
  newDevice->sshServers.emplace_back(newServer);
  PairedDevicesStorage::AddDevice(newDevice.value());
  return {};
}

bool SshServerHelper::RemoveServer(const std::string &deviceId, int serverIndex) {
  auto device = PairedDevicesStorage::GetDeviceByID(deviceId);
  if(!device.has_value() || serverIndex < 0 || serverIndex >= (int)device->sshServers.size())
    return false;
  device->sshServers.erase(device->sshServers.begin() + serverIndex);
  PairedDevicesStorage::AddDevice(device.value());
  return true;
}
