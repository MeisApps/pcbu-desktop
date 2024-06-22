#ifndef PCBU_DESKTOP_PAMHELPER_H
#define PCBU_DESKTOP_PAMHELPER_H

#include <string>
#include <functional>
#include <filesystem>
#include <spdlog/spdlog.h>

class PAMHelper {
public:
    explicit PAMHelper(const std::function<void(const std::string&)>& logCallback = [](const std::string& str){ spdlog::info(str); });

    static bool HasConfigEntry(const std::string& configName, const std::string& entry);
    void SetConfigEntry(const std::string& configName, const std::string& entry, bool enabled);

private:
    static bool IsConfigGenerated(const std::filesystem::path& filePath);

    void AddToConfig(const std::string& configName, const std::string& entry);
    void RemoveFromConfig(const std::string& configName, const std::string& entry);

    void AddEntryToFile(const std::filesystem::path& filePath, const std::string& entry);
    void RemoveEntryFromFile(const std::filesystem::path& filePath, const std::string& entry);

    std::function<void(const std::string&)> m_Logger{};
};

#endif //PCBU_DESKTOP_PAMHELPER_H
