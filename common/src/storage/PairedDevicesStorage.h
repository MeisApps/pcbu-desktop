#ifndef PCBU_DESKTOP_PAIREDDEVICESSTORAGE_H
#define PCBU_DESKTOP_PAIREDDEVICESSTORAGE_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "PairingMethod.h"

struct PairedDevice {
  std::string id{};
  PairingMethod pairingMethod{};
  std::string deviceName{};
  std::string userName{};
  std::string passwordEnc{};
  std::string encryptionKey{};

  std::string ipAddress{};
  uint16_t tcpPort{};
  uint16_t udpPort{};
  uint16_t udpManualPort{};
  std::string bluetoothAddress{};
  std::string cloudToken{};
};

class PairedDevicesStorage {
public:
  static std::optional<PairedDevice> GetDeviceByID(const std::string &id);
  static std::vector<PairedDevice> GetDevicesForUser(const std::string &userName);

  static void AddDevice(const PairedDevice &device);
  static void RemoveDevice(const std::string &id);

  static std::vector<PairedDevice> GetDevices();
  static void SaveDevices(const std::vector<PairedDevice> &devices);

private:
  static void ProtectFile(const std::string &filePath, bool protect);
#ifdef WINDOWS
  static bool ModifyFileAccess(const std::string &filePath, const std::string &sid, bool deny);
#endif

  static constexpr std::string_view DEVICES_FILE_NAME = "paired_devices.json";
};

#endif // PCBU_DESKTOP_PAIREDDEVICESSTORAGE_H
