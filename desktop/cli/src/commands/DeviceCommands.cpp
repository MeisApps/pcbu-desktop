#include "DeviceCommands.h"

#include <algorithm>
#include <charconv>
#include <iostream>
#include <spdlog/spdlog.h>

#include "cli/CommandLine.h"
#include "handler/UnlockHandler.h"
#include "ssh/SshServerHelper.h"
#include "utils/I18n.h"
#include "utils/StringUtils.h"
#include "utils/TerminalUtils.h"

int DeviceCommands::Devices(const std::vector<std::string> &args) {
  auto cmd = CommandLine(args, {"--yes", "-y"});
  auto action = cmd.GetArg(0);
  if(action.empty() || action == "list") {
    cmd.ExpectArgs(0, 1);
    auto devices = PairedDevicesStorage::GetDevices();
    if(devices.empty()) {
      std::cout << "No paired devices." << std::endl;
      return 0;
    }
    size_t idWidth = 2;
    for(const auto &device : devices)
      idWidth = std::max(idWidth, device.id.size());
    std::cout << fmt::format("{:<{}} {:<24} {:<16} {:<12} {}", "ID", idWidth, "NAME", "USER", "METHOD", "SSH") << std::endl;
    for(const auto &device : devices)
      std::cout << fmt::format("{:<{}} {:<24} {:<16} {:<12} {}", device.id, idWidth, StringUtils::Truncate(device.deviceName, 24),
                               StringUtils::Truncate(device.userName, 16), PairingMethodUtils::ToString(device.pairingMethod),
                               device.sshServers.size())
                << std::endl;
    return 0;
  }
  if(action == "remove") {
    cmd.ExpectArgs(2, 2);
    auto device = FindDevice(cmd.GetArg(1));
    if(!cmd.HasFlag("--yes") && !cmd.HasFlag("-y")) {
      if(!TerminalUtils::IsInteractive())
        throw std::invalid_argument("Pass --yes to remove without confirmation.");
      if(!TerminalUtils::Confirm(fmt::format("Remove '{}' ({})?", device.deviceName, device.id)))
        return 1;
    }
    PairedDevicesStorage::RemoveDevice(device.id);
    std::cout << "Removed." << std::endl;
    return 0;
  }
  throw std::invalid_argument(fmt::format("Unknown action '{}'.", action));
}

int DeviceCommands::Ssh(const std::vector<std::string> &args) {
  auto cmd = CommandLine(args, {"--password-stdin"}, {"--host", "--user", "--key"});
  auto action = cmd.GetArg(0);
  if(action.empty() || action == "list") {
    cmd.ExpectArgs(0, 2);
    if(cmd.GetArgCount() == 2) {
      PrintSshServers(FindDevice(cmd.GetArg(1)));
      return 0;
    }
    auto hasServers = false;
    for(const auto &device : PairedDevicesStorage::GetDevices()) {
      if(device.sshServers.empty())
        continue;
      PrintSshServers(device);
      hasServers = true;
    }
    if(!hasServers)
      std::cout << "No SSH servers." << std::endl;
    return 0;
  }
  if(action == "add") {
    cmd.ExpectArgs(2, 2);
    auto device = FindDevice(cmd.GetArg(1));
    auto server = SshServer();
    server.host = StringUtils::Trim(cmd.GetOption("--host").value_or(""));
    server.user = StringUtils::Trim(cmd.GetOption("--user").value_or(""));
    server.keyPath = StringUtils::Trim(cmd.GetOption("--key").value_or(""));
    if(server.host.empty() && server.keyPath.empty())
      throw std::invalid_argument("Pass --host, --key or both.");

    std::string password{};
    if(cmd.HasFlag("--password-stdin") && !TerminalUtils::IsInteractive()) {
      if(!std::getline(std::cin, password))
        throw std::runtime_error("No password on stdin.");
      if(!password.empty() && password.back() == '\r')
        password.pop_back();
    } else {
      if(!TerminalUtils::IsInteractive())
        throw std::invalid_argument("Missing --password-stdin.");
      auto input = TerminalUtils::ReadPassword(server.keyPath.empty() ? "SSH password: " : "Key passphrase: ");
      if(!input.has_value())
        throw std::runtime_error("Canceled.");
      password = input.value();
    }
    if(password.empty())
      throw std::invalid_argument("The password must not be empty.");

    std::cout << "Confirm on your phone to store the password encrypted with this device." << std::endl;
    auto unlockHandler = UnlockHandler([](const std::string &message) { std::cout << message << std::endl; });
    std::string error{};
    auto finished = TerminalUtils::RunInterruptible([&](std::atomic<bool> &isRunning) {
      error = SshServerHelper::AddServer(unlockHandler, device, server, password, &isRunning);
    });
    if(!finished) {
      std::cout << "Canceled." << std::endl;
      return 1;
    }
    if(!error.empty())
      throw std::runtime_error(error);
    std::cout << "SSH server added." << std::endl;
    return 0;
  }
  if(action == "remove") {
    cmd.ExpectArgs(3, 3);
    auto device = FindDevice(cmd.GetArg(1));
    auto indexStr = cmd.GetArg(2);
    int index{};
    auto [ptr, ec] = std::from_chars(indexStr.data(), indexStr.data() + indexStr.size(), index);
    if(ec != std::errc() || ptr != indexStr.data() + indexStr.size())
      throw std::invalid_argument(fmt::format("Invalid index '{}'.", indexStr));
    if(!SshServerHelper::RemoveServer(device.id, index))
      throw std::runtime_error(fmt::format("No SSH server with index {}.", index));
    std::cout << "SSH server removed." << std::endl;
    return 0;
  }
  throw std::invalid_argument(fmt::format("Unknown action '{}'.", action));
}

int DeviceCommands::Test(const std::vector<std::string> &args) {
  auto cmd = CommandLine(args);
  cmd.ExpectArgs(1, 1);
  auto device = FindDevice(cmd.GetArg(0));
  auto unlockHandler = UnlockHandler([](const std::string &message) { std::cout << message << std::endl; });
  UnlockResult result{};
  auto finished = TerminalUtils::RunInterruptible([&](std::atomic<bool> &isRunning) {
    result = unlockHandler.GetResult(device.userName, I18n::Get("unlock_test"), {device.id}, &isRunning);
  });
  if(!finished) {
    std::cout << I18n::Get("unlock_canceled") << std::endl;
    return 1;
  }
  return result.state == UnlockState::SUCCESS ? 0 : 1;
}

PairedDevice DeviceCommands::FindDevice(const std::string &idOrPrefix) {
  if(idOrPrefix.empty())
    throw std::invalid_argument("Missing device ID.");

  std::vector<PairedDevice> matches{};
  for(const auto &device : PairedDevicesStorage::GetDevices()) {
    if(device.id == idOrPrefix)
      return device;
    if(device.id.starts_with(idOrPrefix))
      matches.push_back(device);
  }
  if(matches.empty())
    throw std::runtime_error(fmt::format("No paired device with ID '{}'.", idOrPrefix));
  if(matches.size() > 1)
    throw std::runtime_error(fmt::format("Device ID '{}' is ambiguous.", idOrPrefix));
  return matches.front();
}

void DeviceCommands::PrintSshServers(const PairedDevice &device) {
  std::cout << fmt::format("{} ({})", device.deviceName, device.id) << std::endl;
  if(device.sshServers.empty()) {
    std::cout << "  No SSH servers." << std::endl;
    return;
  }
  for(size_t i = 0; i < device.sshServers.size(); i++) {
    const auto &server = device.sshServers[i];
    auto target = server.user.empty() ? server.host : fmt::format("{}@{}", server.user, server.host);
    if(!server.keyPath.empty())
      target += fmt::format("{}key: {}", target.empty() ? "" : ", ", server.keyPath);
    std::cout << fmt::format("  [{}] {}", i, target) << std::endl;
  }
}
