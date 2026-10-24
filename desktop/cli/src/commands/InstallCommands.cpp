#include "InstallCommands.h"

#include <algorithm>
#include <boost/dll/runtime_symbol_info.hpp>
#include <iostream>
#include <spdlog/spdlog.h>

#include "cli/CommandLine.h"
#include "installer/AppInstaller.h"
#include "installer/StartupChecks.h"
#include "installer/service/ServiceInstaller.h"
#include "storage/AppSettings.h"
#include "storage/PairedDevicesStorage.h"
#include "updater/Updater.h"
#include "utils/AppInfo.h"
#include "utils/TerminalUtils.h"

#ifndef WINDOWS
#include <unistd.h>
#endif

int InstallCommands::Status(const std::vector<std::string> &args) {
  CommandLine(args).ExpectArgs(0, 0);
  auto isInstalled = ServiceInstaller::IsInstalled();
  auto installedVersion = AppSettings::Get().installedVersion;
  std::cout << fmt::format("Version:          {}", AppInfo::GetVersion()) << std::endl;
  std::cout << fmt::format("Installed:        {}", isInstalled ? fmt::format("yes ({})", installedVersion.empty() ? "unknown" : installedVersion) : "no")
            << std::endl;
#ifdef APPLE
  if(isInstalled && StartupChecks::NeedsReinstall())
    std::cout << "Reinstall needed: yes, open the PulseUnlock app" << std::endl;
#else
  if(isInstalled && StartupChecks::NeedsReinstall())
    std::cout << "Reinstall needed: yes, run 'pulseunlock reinstall'" << std::endl;
#endif
  std::cout << fmt::format("Paired devices:   {}", PairedDevicesStorage::GetDevices().size()) << std::endl;
  if(!isInstalled)
    return 0;

  std::cout << std::endl << "Service settings:" << std::endl;
  auto settings = ServiceInstaller().GetSettings();
  size_t idWidth = 2;
  size_t valueWidth = 3;
  for(const auto &setting : settings) {
    idWidth = std::max(idWidth, setting.id.size());
    valueWidth = std::max(valueWidth, setting.selectedValue.size());
  }
  for(const auto &setting : settings) {
    auto value = setting.type == "toggle" ? (setting.enabled ? "on" : "off") : setting.selectedValue;
    std::cout << fmt::format("  {:<{}} {:<{}} {}", setting.id, idWidth, value, valueWidth, setting.name) << std::endl;
  }
  return 0;
}

int InstallCommands::Install(const std::vector<std::string> &args) {
  CommandLine(args).ExpectArgs(0, 0);
#ifdef APPLE
  throw std::runtime_error("Installing from the CLI is not supported on macOS. Use the PulseUnlock app instead.");
#else
  if(ServiceInstaller::IsInstalled())
    throw std::runtime_error("Already installed. Use 'pulseunlock reinstall' to update the installation.");
  AppInstaller(PrintLog).Install();
  return 0;
#endif
}

int InstallCommands::Reinstall(const std::vector<std::string> &args) {
  CommandLine(args).ExpectArgs(0, 0);
#ifdef APPLE
  throw std::runtime_error("Reinstalling from the CLI is not supported on macOS. Use the PulseUnlock app instead.");
#else
  if(!ServiceInstaller::IsInstalled())
    throw std::runtime_error("Not installed. Use 'pulseunlock install' instead.");
  AppInstaller(PrintLog).Reinstall();
  return 0;
#endif
}

int InstallCommands::Uninstall(const std::vector<std::string> &args) {
  auto cmd = CommandLine(args, {"--yes", "-y"});
  cmd.ExpectArgs(0, 0);
  if(!ServiceInstaller::IsInstalled())
    throw std::runtime_error("Not installed.");
  if(!cmd.HasFlag("--yes") && !cmd.HasFlag("-y")) {
    if(!TerminalUtils::IsInteractive())
      throw std::invalid_argument("Pass --yes to uninstall without confirmation.");
    if(!TerminalUtils::Confirm("This removes all modules, their configuration and the pulseunlock command. Continue?"))
      return 1;
  }
  AppInstaller(PrintLog).Uninstall();
  return 0;
}

int InstallCommands::Update(const std::vector<std::string> &args) {
  auto cmd = CommandLine(args, {"--check", "--force"});
  cmd.ExpectArgs(0, 0);
  auto currentVersion = AppInfo::GetVersion();
  auto latestVersion = Updater::GetLatestVersion();
  auto hasUpdate = AppInfo::CompareVersion(currentVersion, latestVersion) == 1;
  std::cout << fmt::format("Current version: {}", currentVersion) << std::endl;
  std::cout << fmt::format("Latest version:  {}", latestVersion) << std::endl;
  if(!hasUpdate) {
    std::cout << "Already up to date." << std::endl;
    return 0;
  }
  if(cmd.HasFlag("--check"))
    return 0;

#ifdef WINDOWS
  throw std::runtime_error("Updating the CLI is not supported on Windows.");
#elif APPLE
  throw std::runtime_error("Updating the CLI is not supported on macOS. Update the PulseUnlock app instead.");
#else
#ifndef STANDALONE_CLI
  if(!cmd.HasFlag("--force"))
    throw std::runtime_error("This CLI was installed by the PulseUnlock app. Update the app instead, or pass --force.");
#endif
  auto exePath = boost::dll::program_location().string();
  auto isCanceled = false;
  TerminalUtils::RunInterruptible([&](std::atomic<bool> &isRunning) {
    try {
      Updater::InstallCliUpdate(exePath, [&](uint64_t len, uint64_t total) {
        auto lenMib = (float)len / 1048576.F;
        if(total > 0)
          std::cout << fmt::format("\rDownloading... {}% ({:.2f}MiB / {:.2f}MiB)", len * 100 / total, lenMib, (float)total / 1048576.F) << std::flush;
        else
          std::cout << fmt::format("\rDownloading... {:.2f}MiB", lenMib) << std::flush;
        return isRunning.load();
      });
    } catch(const std::exception &) {
      if(isRunning)
        throw;
      isCanceled = true;
    }
  });
  if(isCanceled) {
    std::cout << std::endl << "Canceled." << std::endl;
    return 1;
  }
  std::cout << std::endl << fmt::format("Updated to {}.", latestVersion) << std::endl;
  if(!ServiceInstaller::IsInstalled())
    return 0;

  std::cout << "Reinstalling modules..." << std::endl;
  spdlog::default_logger()->flush();
  execl(exePath.c_str(), exePath.c_str(), "reinstall", nullptr);
  throw std::runtime_error("Failed to start the new version. Run 'pulseunlock reinstall' manually.");
#endif
}

void InstallCommands::PrintLog(const std::string &message) {
  spdlog::info(message);
  std::cout << message << std::endl;
}
