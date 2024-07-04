#ifndef PCBU_DESKTOP_APPSTORAGE_H
#define PCBU_DESKTOP_APPSTORAGE_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct PCBUAppStorage {
    std::string installedVersion{};
    std::string language{};
    std::string serverIP{};
    std::string serverMAC{};
    uint16_t serverPort{};
    uint32_t socketTimeout{};
    bool waitForKeyPress{};
};

class AppSettings {
public:
    static const std::filesystem::path BASE_DIR;

    static PCBUAppStorage Get();
    static void Save(const PCBUAppStorage& storage);

    static void SetInstalledVersion(bool isInstall);

private:
    static constexpr std::string_view SETTINGS_FILE_NAME = "app_settings.json";
};

#endif //PCBU_DESKTOP_APPSTORAGE_H
