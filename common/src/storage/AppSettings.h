#ifndef PCBU_DESKTOP_APPSTORAGE_H
#define PCBU_DESKTOP_APPSTORAGE_H

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

struct PCBUAppStorage {
  std::string machineID{};
  std::string installedVersion{};
  std::string language{};
  std::string serverIP{};
  std::string serverMAC{};
  uint16_t pairingServerPort{};
  uint16_t unlockServerPort{};
  uint32_t clientSocketTimeout{};
  uint32_t clientConnectTimeout{};
  uint32_t clientConnectRetries{};

  bool winWaitForKeyPress{};
  bool winHidePasswordField{};
};

class AppSettings {
public:
  static std::filesystem::path GetBaseDir();

  static PCBUAppStorage Get();
  static void Save(const PCBUAppStorage &storage);

  static void InvalidateCache();
  static void SetInstalledVersion(bool isInstalled);

private:
  static PCBUAppStorage Load();

  static PCBUAppStorage g_Cache;
  static std::mutex g_Mutex;

  static constexpr std::string_view SETTINGS_FILE_NAME = "app_settings.json";
};

#endif // PCBU_DESKTOP_APPSTORAGE_H
