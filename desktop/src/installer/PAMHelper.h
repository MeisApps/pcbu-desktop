#ifndef PCBU_DESKTOP_PAMHELPER_H
#define PCBU_DESKTOP_PAMHELPER_H

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>

class PAMHelper {
public:
  explicit PAMHelper(const std::function<void(const std::string &)> &logCallback = [](const std::string &str) { spdlog::info(str); });

  static bool HasConfigEntry(const std::string &configName, const std::string &entry);

  void SetConfigEntry(const std::string &configName, const std::string &entry, bool enabled);
  void MigrateConfigEntry(const std::string &configName, const std::string &oldEntry, const std::string &newEntry);
  void Commit();

private:
  bool ConfigExists(const std::filesystem::path &filePath);
  std::string ReadConfig(const std::filesystem::path &filePath);

  bool IsConfigGenerated(const std::filesystem::path &filePath);

  void AddToConfig(const std::string &configName, const std::string &entry);
  void RemoveFromConfig(const std::string &configName, const std::string &entry);

  void AddEntryToFile(const std::filesystem::path &filePath, const std::string &entry);
  void RemoveEntryFromFile(const std::filesystem::path &filePath, const std::string &entry);

  bool SetPolkitRestrictions(bool enabled);

#ifdef APPLE
  void CommitWithTerminal(const std::map<std::filesystem::path, std::optional<std::string>> &changes);
#endif

  std::function<void(const std::string &)> m_Logger{};
  std::map<std::filesystem::path, std::optional<std::string>> m_PendingChanges{};
};

#endif // PCBU_DESKTOP_PAMHELPER_H
