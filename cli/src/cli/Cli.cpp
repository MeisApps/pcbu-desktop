#include "Cli.h"

#include <functional>
#include <iostream>
#include <map>
#include <spdlog/spdlog.h>
#include <stdexcept>

#include "commands/DeviceCommands.h"
#include "commands/InstallCommands.h"
#include "commands/PairingCommand.h"
#include "commands/SettingsCommands.h"
#include "installer/StartupChecks.h"
#include "shell/LocalShell.h"
#include "shell/Shell.h"
#include "storage/LoggingSystem.h"
#include "utils/AppInfo.h"
#include "utils/TerminalUtils.h"

int Cli::Run(int argc, char *argv[]) {
  spdlog::set_level(spdlog::level::off);
  TerminalUtils::Init();
  std::vector<std::string> args(argv + 1, argv + argc);
  auto command = args.empty() ? std::string("help") : args.front();
  if(!args.empty())
    args.erase(args.begin());

  if(command == "help" || command == "--help" || command == "-h") {
    PrintHelp();
    return 0;
  }
  if(command == "version" || command == "--version") {
    PrintVersion();
    return 0;
  }

  auto result = 1;
  try {
    auto commands = GetCommands();
    auto it = commands.find(command);
    if(it == commands.end())
      throw std::invalid_argument(fmt::format("Unknown command '{}'.", command));
    CheckEnvironment();
    LoggingSystem::Init("cli", false, true);
    spdlog::info("Running command '{}'.", command);
    result = it->second(args);
  } catch(const std::invalid_argument &ex) {
    std::cerr << fmt::format("Error: {}", ex.what()) << std::endl;
    std::cerr << "Run 'pulseunlock help' for usage." << std::endl;
    result = 2;
  } catch(const std::exception &ex) {
    spdlog::error("Command '{}' failed: {}", command, ex.what());
    std::cerr << fmt::format("Error: {}", ex.what()) << std::endl;
    result = 1;
  }
  Shell::Destroy();
  LoggingSystem::Destroy();
  return result;
}

std::map<std::string, std::function<int(const std::vector<std::string> &)>> Cli::GetCommands() {
  return {
      {"status", InstallCommands::Status},       {"install", InstallCommands::Install},   {"reinstall", InstallCommands::Reinstall},
      {"uninstall", InstallCommands::Uninstall}, {"update", InstallCommands::Update},     {"pair", PairingCommand::Run},
      {"devices", DeviceCommands::Devices},      {"ssh", DeviceCommands::Ssh},            {"test", DeviceCommands::Test},
      {"config", SettingsCommands::Config},      {"service", SettingsCommands::Service}, {"logs", SettingsCommands::Logs},
      {"debug-log", SettingsCommands::DebugLog},
  };
}

void Cli::CheckEnvironment() {
  if(!LocalShell::IsRunningAsAdmin()) {
#ifdef WINDOWS
    throw std::runtime_error("This command must be run from an administrator terminal.");
#else
    throw std::runtime_error("This command must be run as root, e.g. 'sudo pulseunlock ...'.");
#endif
  }
  if(auto error = StartupChecks::Run())
    throw std::runtime_error(error.value());
}

void Cli::PrintHelp() {
  std::cout << R"(Usage: pulseunlock <command> [options]

Setup:
  status                              Show the installation state and service settings
  install                             Install the modules and apply the default settings
  reinstall                           Reinstall the modules, keeping the settings
  uninstall [--yes]                   Remove the modules, their configuration and the CLI
  update [--check] [--force]          Update the CLI and reinstall the modules
    --force                           Update a CLI installed by the desktop app

Pairing:
  pair [options]                      Pair a phone, prompting for anything not passed
    --user <name>                     User to unlock
    --password-stdin                  Read the user's password from stdin
    --skip-password-check             Pair even if the password can't be verified
    --method <method>                 cloud, udp, tcp, bt or manual
    --legacy                          Use legacy pairing (not with cloud)
    --bt-address <address>            Phone's Bluetooth address (implies bt)
  devices [list]                      List paired devices
  devices remove <id> [--yes]         Remove a paired device (ID prefixes work)
  test <id>                           Run an unlock test with a paired device

SSH:
  ssh [list [<id>]]                   List SSH servers
  ssh add <id> [options]              Add an SSH server, confirmed on the phone
    --host <host>                     Server host
    --user <user>                     Server user
    --key <path>                      Private key path
    --password-stdin                  Read the password or passphrase from stdin
  ssh remove <id> <index>             Remove an SSH server

Settings:
  config [list]                       Show all settings
  config get <key>                    Show one setting
  config set <key> <value> [--yes]    Change a setting
  config interfaces                   List network interfaces for serverIP/serverMAC
  service [list]                      Show service settings and their options
  service set <id> <on|off|value>     Change a service setting
  logs [<name>] [-n <lines>]          Show the logs (cli, desktop, module, elevator)
  debug-log [on|off]                  Show or change debug logging

Other:
  help                                Show this help
  version                             Show the version

All commands except help and version need root (administrator on Windows).
)";
}

void Cli::PrintVersion() {
  std::cout << fmt::format("pulseunlock {}", AppInfo::GetVersion()) << std::endl;
}
