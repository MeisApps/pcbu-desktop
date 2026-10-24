#include "SettingsCommands.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <limits>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

#include "installer/service/ServiceInstaller.h"
#include "platform/NetworkHelper.h"
#include "platform/PlatformHelper.h"
#include "shell/LocalShell.h"
#include "storage/LoggingSystem.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/StringUtils.h"
#include "utils/TerminalUtils.h"

int SettingsCommands::Config(const std::vector<std::string> &args) {
  auto cmd = CommandLine(args, {"--yes", "-y"});
  auto action = cmd.GetArg(0);
  auto settings = AppSettings::Get();
  if(action.empty() || action == "list") {
    cmd.ExpectArgs(0, 1);
    for(const auto &key : GetConfigKeys())
      std::cout << fmt::format("{:<22} {}", key, GetConfigValue(settings, key)) << std::endl;
    return 0;
  }
  if(action == "get") {
    cmd.ExpectArgs(2, 2);
    std::cout << GetConfigValue(settings, cmd.GetArg(1)) << std::endl;
    return 0;
  }
  if(action == "set") {
    cmd.ExpectArgs(3, 3);
    SetConfigValue(settings, cmd.GetArg(1), cmd.GetArg(2), cmd);
    AppSettings::Save(settings);
    AppSettings::InvalidateCache();
    return 0;
  }
  if(action == "interfaces") {
    cmd.ExpectArgs(1, 1);
    std::cout << fmt::format("{:<16} {:<40} {}", "INTERFACE", "IP", "MAC") << std::endl;
    for(const auto &netIf : NetworkHelper::GetLocalNetInterfaces())
      std::cout << fmt::format("{:<16} {:<40} {}", netIf.ifName, netIf.ipAddress, netIf.macAddress) << std::endl;
    return 0;
  }
  throw std::invalid_argument(fmt::format("Unknown action '{}'.", action));
}

int SettingsCommands::Service(const std::vector<std::string> &args) {
  auto cmd = CommandLine(args);
  auto action = cmd.GetArg(0);
  auto installer = ServiceInstaller([](const std::string &message) {
    spdlog::info(message);
    std::cout << message << std::endl;
  });
  auto settings = installer.GetSettings();
  if(action.empty() || action == "list") {
    cmd.ExpectArgs(0, 1);
    size_t idWidth = 2;
    size_t valueWidth = 3;
    for(const auto &setting : settings) {
      idWidth = std::max(idWidth, setting.id.size());
      valueWidth = std::max(valueWidth, setting.selectedValue.size());
      for(const auto &option : setting.options)
        valueWidth = std::max(valueWidth, option.value.size() + 2);
    }
    for(const auto &setting : settings) {
      if(setting.type == "toggle") {
        std::cout << fmt::format("{:<{}} {:<{}} {}", setting.id, idWidth, setting.enabled ? "on" : "off", valueWidth, setting.name) << std::endl;
        continue;
      }
      std::cout << fmt::format("{:<{}} {:<{}} {}", setting.id, idWidth, setting.selectedValue, valueWidth, setting.name) << std::endl;
      for(const auto &option : setting.options)
        std::cout << fmt::format("{:<{}}   {:<{}} {}", "", idWidth, option.value, valueWidth - 2, option.name) << std::endl;
    }
    return 0;
  }
  if(action == "set") {
    cmd.ExpectArgs(3, 3);
    auto id = cmd.GetArg(1);
    auto value = StringUtils::ToLower(cmd.GetArg(2));
    auto it = std::ranges::find_if(settings, [&](const ServiceSetting &setting) { return setting.id == id; });
    if(it == settings.end())
      throw std::invalid_argument(fmt::format("Unknown service setting '{}'.", id));
    if(it->type == "toggle") {
      if(value == "on" || value == "true" || value == "1")
        it->enabled = true;
      else if(value == "off" || value == "false" || value == "0")
        it->enabled = false;
      else
        throw std::invalid_argument("Use on or off.");
    } else {
      if(std::ranges::none_of(it->options, [&](const ServiceSettingOption &option) { return option.value == cmd.GetArg(2); }))
        throw std::invalid_argument(fmt::format("Invalid value '{}'. Run 'pulseunlock service list' for the options.", cmd.GetArg(2)));
      it->selectedValue = cmd.GetArg(2);
    }
    installer.ApplySettings(settings, false);
    AppSettings::InvalidateCache();
    std::cout << fmt::format("{} is now {}.", id, it->type == "toggle" ? (it->enabled ? "on" : "off") : it->selectedValue) << std::endl;
    return 0;
  }
  throw std::invalid_argument(fmt::format("Unknown action '{}'.", action));
}

int SettingsCommands::Logs(const std::vector<std::string> &args) {
  auto cmd = CommandLine(args, {}, {"-n"});
  cmd.ExpectArgs(0, 1);
  auto lineCount = ParseNumber<size_t>("-n", cmd.GetOption("-n").value_or("50"));
  std::vector<std::string> logNames = {"cli", "desktop", "module", "elevator"};
#ifdef APPLE
  logNames.emplace_back("mac_agent");
#endif
  if(cmd.GetArgCount() == 1) {
    if(std::ranges::find(logNames, cmd.GetArg(0)) == logNames.end())
      throw std::invalid_argument(fmt::format("Unknown log '{}'. Use one of: {}.", cmd.GetArg(0), fmt::join(logNames, ", ")));
    PrintLogTail(cmd.GetArg(0), lineCount, false);
    return 0;
  }
  for(const auto &logName : logNames)
    PrintLogTail(logName, lineCount, true);
  return 0;
}

int SettingsCommands::DebugLog(const std::vector<std::string> &args) {
  auto cmd = CommandLine(args);
  cmd.ExpectArgs(0, 1);
  auto action = cmd.GetArg(0);
  if(action.empty()) {
    std::cout << (LoggingSystem::IsDebugEnabled() ? "on" : "off") << std::endl;
    return 0;
  }
  if(action != "on" && action != "off")
    throw std::invalid_argument("Use on or off.");
  if(!LoggingSystem::SetDebugEnabled(action == "on"))
    throw std::runtime_error("Failed to change debug logging.");
  return 0;
}

std::vector<std::string> SettingsCommands::GetConfigKeys() {
  return {"machineID",         "installedVersion", "language",           "serverIP",           "serverMAC",           "pairingDiscoveryPort",
          "pairingServerPort", "unlockServerPort", "clientSocketTimeout", "clientConnectTimeout", "clientConnectRetries"};
}

std::string SettingsCommands::GetConfigValue(const PCBUAppStorage &settings, const std::string &key) {
  if(key == "machineID")
    return settings.machineID;
  if(key == "installedVersion")
    return settings.installedVersion;
  if(key == "language")
    return settings.language;
  if(key == "serverIP")
    return settings.serverIP;
  if(key == "serverMAC")
    return settings.serverMAC;
  if(key == "pairingDiscoveryPort")
    return std::to_string(settings.pairingDiscoveryPort);
  if(key == "pairingServerPort")
    return std::to_string(settings.pairingServerPort);
  if(key == "unlockServerPort")
    return std::to_string(settings.unlockServerPort);
  if(key == "clientSocketTimeout")
    return std::to_string(settings.clientSocketTimeout);
  if(key == "clientConnectTimeout")
    return std::to_string(settings.clientConnectTimeout);
  if(key == "clientConnectRetries")
    return std::to_string(settings.clientConnectRetries);
  throw std::invalid_argument(fmt::format("Unknown key '{}'.", key));
}

void SettingsCommands::SetConfigValue(PCBUAppStorage &settings, const std::string &key, const std::string &value, const CommandLine &cmd) {
  if(key == "language") {
    std::vector<std::string> languages = {"auto", "en", "de", "zh_CN", "pt_PT", "pt_BR"};
    if(std::ranges::find(languages, value) == languages.end())
      throw std::invalid_argument(fmt::format("Unknown language '{}'. Use one of: {}.", value, fmt::join(languages, ", ")));
    settings.language = value;
  } else if(key == "serverIP") {
    if(value.empty())
      throw std::invalid_argument("Use auto or an IP address.");
    settings.serverIP = value;
    if(value != "auto")
      settings.serverMAC = {};
  } else if(key == "serverMAC") {
    auto interfaces = NetworkHelper::GetLocalNetInterfaces();
    if(std::ranges::none_of(interfaces, [&](const NetworkInterface &netIf) { return netIf.macAddress == value; }))
      throw std::invalid_argument(fmt::format("No interface with MAC '{}'. Run 'pulseunlock config interfaces'.", value));
    settings.serverMAC = value;
  } else if(key == "pairingDiscoveryPort") {
    settings.pairingDiscoveryPort = ParseNumber<uint16_t>(key, value);
  } else if(key == "pairingServerPort") {
    settings.pairingServerPort = ParseNumber<uint16_t>(key, value);
  } else if(key == "unlockServerPort") {
    auto port = ParseNumber<uint16_t>(key, value);
    auto hasUDPDevices =
        PairedDevicesStorage::HasDevicesWithMethod(PairingMethod::UDP) || PairedDevicesStorage::HasDevicesWithMethod(PairingMethod::MANUAL_UDP);
    if(port != settings.unlockServerPort && hasUDPDevices) {
      if(!cmd.HasFlag("--yes") && !cmd.HasFlag("-y")) {
        if(!TerminalUtils::IsInteractive())
          throw std::invalid_argument("Changing this port removes all UDP paired devices. Pass --yes to confirm.");
        if(!TerminalUtils::Confirm("Changing this port removes all UDP paired devices. Continue?"))
          throw std::runtime_error("Canceled.");
      }
      PairedDevicesStorage::RemoveDevicesWithMethod(PairingMethod::UDP);
      PairedDevicesStorage::RemoveDevicesWithMethod(PairingMethod::MANUAL_UDP);
    }
    settings.unlockServerPort = port;
  } else if(key == "clientSocketTimeout") {
    settings.clientSocketTimeout = ParseNumber<uint32_t>(key, value);
  } else if(key == "clientConnectTimeout") {
    settings.clientConnectTimeout = ParseNumber<uint32_t>(key, value);
  } else if(key == "clientConnectRetries") {
    settings.clientConnectRetries = ParseNumber<uint32_t>(key, value);
  } else if(key == "machineID" || key == "installedVersion") {
    throw std::invalid_argument(fmt::format("'{}' is read-only.", key));
  } else {
    throw std::invalid_argument(fmt::format("Unknown key '{}'.", key));
  }
}

template <typename T> T SettingsCommands::ParseNumber(const std::string &key, const std::string &value) {
  T result{};
  auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
  if(ec != std::errc() || ptr != value.data() + value.size())
    throw std::invalid_argument(fmt::format("Invalid value '{}' for {}. Expected a number up to {}.", value, key, std::numeric_limits<T>::max()));
  return result;
}

void SettingsCommands::PrintLogTail(const std::string &logName, size_t lineCount, bool withHeader) {
  auto logsDir = AppSettings::GetLogsDir(false);
#ifdef APPLE
  if(logName == "desktop")
    logsDir = PlatformHelper::GetUserHomeDir(PlatformHelper::GetCurrentUser()) / "Library/Logs/PulseUnlock";
#endif
  auto logPath = logsDir / fmt::format("{}.log", logName);
  if(!std::filesystem::exists(logPath)) {
    if(!withHeader)
      std::cout << fmt::format("No log at '{}'.", logPath.string()) << std::endl;
    return;
  }
  spdlog::default_logger()->flush();
  auto data = LocalShell::ReadBytes(logPath);
  auto lines = StringUtils::Split(std::string(data.begin(), data.end()), "\n");
  while(!lines.empty() && StringUtils::Trim(lines.back()).empty())
    lines.pop_back();
  auto start = lines.size() > lineCount ? lines.size() - lineCount : 0;
  if(withHeader)
    std::cout << fmt::format("==> {} <==", logPath.string()) << std::endl;
  for(size_t i = start; i < lines.size(); i++)
    std::cout << lines[i] << std::endl;
  if(withHeader)
    std::cout << std::endl;
}
