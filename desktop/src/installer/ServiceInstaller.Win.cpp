#include "ServiceInstaller.h"

#include <Windows.h>

#include "WinFirewallHelper.h"
#include "shell/Shell.h"
#include "storage/AppSettings.h"
#include "utils/ResourceHelper.h"

#define LIB_MODULE_DIR std::filesystem::path("C:\\Windows\\System32")
#define LIB_MODULE_FILE "win-pcbiounlock.dll"

#define CRED_PROVIDER_NAME "win-pcbiounlock"
#define CRED_PROVIDER_GUID "74A23DE2-B81D-46EC-E129-CD32507ED716"
#define APP_FIREWALL_RULE_NAME "PC Bio Unlock"

ServiceInstaller::ServiceInstaller(const std::function<void(const std::string &)> &logCallback) {
    m_Logger = logCallback;
}

std::vector<ServiceSetting> ServiceInstaller::GetSettings() {
    return {{"waitForKey", I18n::Get("service_setting_wait_for_key"), AppSettings::Get().waitForKeyPress, true}};
}

void ServiceInstaller::ApplySettings(const std::vector<ServiceSetting> &settings, bool useDefault) {
    for(auto setting : settings) {
        auto isEnabled = useDefault ? setting.defaultVal : setting.enabled;
        if(setting.id == "waitForKey") {
            auto storage = AppSettings::Get();
            storage.waitForKeyPress = isEnabled;
            AppSettings::Save(storage);
        } else {
            spdlog::warn("Unknown service setting {}.", setting.id);
        }
    }
}

bool ServiceInstaller::IsInstalled() {
    return std::filesystem::exists(LIB_MODULE_DIR / LIB_MODULE_FILE);
}

void ServiceInstaller::Install() {
    m_Logger("Copying credential provider...");
    auto nativeLib = ResourceHelper::GetResource(":/res/natives/{}", LIB_MODULE_FILE);
    auto libPath = LIB_MODULE_DIR / LIB_MODULE_FILE;
    auto result = Shell::WriteBytes(libPath, nativeLib);
    if(!result)
        throw std::runtime_error(I18n::Get("error_file_write", libPath.string()));

    m_Logger("Creating registry entries...");
    result = Shell::RunCommand(fmt::format(R"(reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{0}" /t REG_SZ /d {1} /f)", CRED_PROVIDER_GUID, CRED_PROVIDER_NAME)).exitCode == 0 &&
            Shell::RunCommand(fmt::format(R"(reg add "HKEY_CLASSES_ROOT\CLSID\{0}" /t REG_SZ /d {1} /f)", CRED_PROVIDER_GUID, CRED_PROVIDER_NAME)).exitCode == 0 &&
            Shell::RunCommand(fmt::format(R"(reg add "HKEY_CLASSES_ROOT\CLSID\{0}\InprocServer32" /t REG_SZ /d {1} /f)", CRED_PROVIDER_GUID, CRED_PROVIDER_NAME)).exitCode == 0 &&
            Shell::RunCommand(fmt::format(R"(reg add "HKEY_CLASSES_ROOT\CLSID\{0}\InprocServer32" /t REG_SZ /v ThreadingModel /d Apartment /f)", CRED_PROVIDER_GUID)).exitCode == 0;
    if(!result)
        throw std::runtime_error(I18n::Get("error_registry_add"));

    CHAR exePath[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if(std::filesystem::exists(exePath)) {
        m_Logger("Removing old firewall rules...");
        WinFirewallHelper::RemoveAllRulesForProgram(exePath);
        m_Logger("Adding Windows firewall rule...");
        result = Shell::RunCommand(fmt::format(R"(netsh advfirewall firewall add rule name="{0}" dir=in program="{1}" profile=any action=allow)", APP_FIREWALL_RULE_NAME, exePath)).exitCode == 0;
        if(!result)
            m_Logger(I18n::Get("warning_firewall_rule_add", "Windows Firewall"));
    } else {
        m_Logger("Warning: App path not found. Skipped adding firewall rule.");
    }
    m_Logger("Done.");
}

void ServiceInstaller::Uninstall() {
    m_Logger("Removing credential provider...");
    auto libPath = LIB_MODULE_DIR / LIB_MODULE_FILE;
    auto result = Shell::RemoveFile(libPath);
    if(!result)
        throw std::runtime_error(I18n::Get("error_file_remove", libPath.string()));

    m_Logger("Removing registry entries...");
    result = Shell::RunCommand(fmt::format(R"(reg delete "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{0}" /f)", CRED_PROVIDER_GUID)).exitCode == 0 &&
             Shell::RunCommand(fmt::format(R"(reg delete "HKEY_CLASSES_ROOT\CLSID\{0}" /f)", CRED_PROVIDER_GUID)).exitCode == 0;
    if(!result)
        throw std::runtime_error(I18n::Get("error_registry_remove"));

    m_Logger("Removing Windows firewall rule...");
    result = Shell::RunCommand(fmt::format(R"(netsh advfirewall firewall delete rule name="{0}")", APP_FIREWALL_RULE_NAME)).exitCode == 0;
    if(!result)
        m_Logger(I18n::Get("warning_firewall_rule_remove", "Windows Firewall"));
    m_Logger("Done.");
}
