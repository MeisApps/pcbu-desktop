#ifndef PCBU_CLI_SETTINGSCOMMANDS_H
#define PCBU_CLI_SETTINGSCOMMANDS_H

#include <string>
#include <vector>

#include "cli/CommandLine.h"
#include "storage/AppSettings.h"

class SettingsCommands {
public:
  static int Config(const std::vector<std::string> &args);
  static int Service(const std::vector<std::string> &args);
  static int Logs(const std::vector<std::string> &args);
  static int DebugLog(const std::vector<std::string> &args);

private:
  SettingsCommands() = default;

  static std::vector<std::string> GetConfigKeys();
  static std::string GetConfigValue(const PCBUAppStorage &settings, const std::string &key);
  static void SetConfigValue(PCBUAppStorage &settings, const std::string &key, const std::string &value, const CommandLine &cmd);
  template <typename T> static T ParseNumber(const std::string &key, const std::string &value);

  static void PrintLogTail(const std::string &logName, size_t lineCount, bool withHeader);
};

#endif // PCBU_CLI_SETTINGSCOMMANDS_H
