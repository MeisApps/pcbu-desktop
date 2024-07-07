#include "AppSettings.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "shell/Shell.h"
#include "utils/AppInfo.h"

#ifdef WINDOWS
const std::filesystem::path AppSettings::BASE_DIR = {"C:\\ProgramData\\PCBioUnlock"};
#else
const std::filesystem::path AppSettings::BASE_DIR = {"/etc/pc-bio-unlock"};
#endif

PCBUAppStorage AppSettings::Get() {
    try {
        auto jsonData = Shell::ReadBytes(BASE_DIR / SETTINGS_FILE_NAME);
        auto json = nlohmann::json::parse(jsonData);
        auto settings = PCBUAppStorage();
        settings.installedVersion = json["installedVersion"];
        settings.language = json["language"];
        settings.serverIP = json["serverIP"];
        settings.serverMAC = json["serverMAC"];
        settings.serverPort = json["serverPort"];
        settings.clientSocketTimeout = json["clientSocketTimeout"];
        settings.clientConnectRetries = json["clientConnectRetries"];
        settings.waitForKeyPress = json["waitForKeyPress"];
        return settings;
    } catch (const std::exception& ex) {
        spdlog::error("Failed reading app storage: {}", ex.what());
        auto def = PCBUAppStorage();
        def.language = "auto";
        def.serverIP = "auto";
        def.serverPort = 43295;
        def.clientSocketTimeout = 30;
        def.clientConnectRetries = 0;
        def.waitForKeyPress = false;
        spdlog::info("Creating new app storage...");
        Save(def);
        return def;
    }
}

void AppSettings::Save(const PCBUAppStorage &storage) {
    try {
        nlohmann::json json = {
                {"installedVersion", storage.installedVersion},
                {"language", storage.language},
                {"serverIP", storage.serverIP},
                {"serverMAC", storage.serverMAC},
                {"serverPort", storage.serverPort},
                {"clientSocketTimeout", storage.clientSocketTimeout},
                {"clientConnectRetries", storage.clientConnectRetries},
                {"waitForKeyPress", storage.waitForKeyPress},
        };
        if(!std::filesystem::exists(BASE_DIR))
            Shell::CreateDir(BASE_DIR);
        auto jsonStr = json.dump();
        Shell::WriteBytes(BASE_DIR / SETTINGS_FILE_NAME, {jsonStr.begin(), jsonStr.end()});
    } catch (const std::exception& ex) {
        spdlog::error("Failed writing app storage: {}", ex.what());
    }
}

void AppSettings::SetInstalledVersion(bool isInstall) {
    auto settings = AppSettings::Get();
    settings.installedVersion = isInstall ? AppInfo::GetVersion() : "";
    AppSettings::Save(settings);
}
