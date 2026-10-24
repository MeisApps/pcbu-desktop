#include "PairingCommand.h"

#include <algorithm>
#include <condition_variable>
#include <iostream>
#include <mutex>

#include "installer/service/ServiceInstaller.h"
#include "pairing/PairingSession.h"
#include "platform/BluetoothHelper.h"
#include "platform/PlatformHelper.h"
#include "utils/I18n.h"
#include "utils/QRUtils.h"
#include "utils/TerminalUtils.h"

int PairingCommand::Run(const std::vector<std::string> &args) {
  auto cmd = CommandLine(args, {"--legacy", "--password-stdin", "--skip-password-check"}, {"--user", "--method", "--bt-address"});
  cmd.ExpectArgs(0, 0);
  if(cmd.HasFlag("--legacy") && cmd.GetOption("--method") == "cloud")
    throw std::invalid_argument("--legacy can't be used with the cloud method.");
  if(cmd.GetOption("--bt-address").has_value() && cmd.GetOption("--method").has_value() && cmd.GetOption("--method") != "bt" &&
     cmd.GetOption("--method") != "bluetooth")
    throw std::invalid_argument("--bt-address can only be used with --method bt.");
  if(!ServiceInstaller::IsInstalled())
    throw std::runtime_error(I18n::Get("error_pairing_not_installed"));

  auto isManualUserName = false;
  auto userName = SelectUser(cmd, isManualUserName);
  auto password = ReadPassword(cmd, userName);
  auto check = PairingSession::CheckCredentials(userName, password, isManualUserName, cmd.HasFlag("--skip-password-check"));
  if(!check.error.empty()) {
    if(!check.canSkip)
      throw std::runtime_error(check.error);
    if(!TerminalUtils::IsInteractive())
      throw std::runtime_error(check.error + " Pass --skip-password-check to pair anyway.");
    std::cerr << check.error << std::endl;
    if(!TerminalUtils::Confirm("Pair anyway?"))
      return 1;
  }

  auto data = PairingUIData();
  data.userName = userName;
  data.password = password;
  data.method = SelectMethod(cmd);
  data.useLegacy = cmd.HasFlag("--legacy");
  if(data.useLegacy && data.method == PairingMethod::CLOUD)
    throw std::invalid_argument("--legacy can't be used with the cloud method.");
  if(data.method == PairingMethod::BLUETOOTH) {
    data.btAddress = SelectBluetoothDevice(cmd);
    std::cout << I18n::Get("pairing_form_bt_pair_desc") << std::endl;
    auto device = BluetoothDevice();
    device.address = data.btAddress;
    if(!BluetoothHelper::PairDevice(device))
      throw std::runtime_error(I18n::Get("error_bluetooth_pairing"));
  }

  std::mutex mutex{};
  std::condition_variable condition{};
  auto isDone = false;
  std::string error{};
  auto session = PairingSession();
  session.Start(
      data,
      [&]() {
        std::lock_guard lock(mutex);
        isDone = true;
        condition.notify_all();
      },
      [&](const std::string &errorMsg) {
        std::lock_guard lock(mutex);
        if(!isDone) {
          error = errorMsg;
          isDone = true;
        }
        condition.notify_all();
      });

  std::cout << std::endl << "Open the PulseUnlock app on your phone and scan this QR code:" << std::endl << std::endl;
  std::cout << QRUtils::GenerateTerminal(session.GetPayload()) << std::endl;
  std::cout << fmt::format("Or enter this pairing code: {}", session.GetPairingCode()) << std::endl << std::endl;
  std::cout << "Waiting for your phone... (Ctrl+C to cancel)" << std::endl;
  TerminalUtils::RunInterruptible([&](std::atomic<bool> &isRunning) {
    std::unique_lock lock(mutex);
    while(!isDone && isRunning)
      condition.wait_for(lock, std::chrono::milliseconds(100));
  });
  session.Stop();

  std::lock_guard lock(mutex);
  if(!isDone) {
    std::cout << "Canceled." << std::endl;
    return 1;
  }
  if(!error.empty())
    throw std::runtime_error(error);
  std::cout << "Paired successfully." << std::endl;
  return 0;
}

std::string PairingCommand::SelectUser(const CommandLine &cmd, bool &isManualUserName) {
  auto users = PlatformHelper::GetAllUsers();
  if(auto user = cmd.GetOption("--user")) {
    isManualUserName = std::ranges::find(users, user.value()) == users.end();
    return user.value();
  }
  if(!TerminalUtils::IsInteractive())
    throw std::invalid_argument("Missing --user.");

  auto currentUser = PlatformHelper::GetCurrentUser();
  auto options = users;
  options.emplace_back("Other...");
  auto defaultIndex = (size_t)std::distance(users.begin(), std::ranges::find(users, currentUser));
  if(defaultIndex >= users.size())
    defaultIndex = 0;
  auto index = TerminalUtils::Choose("User to unlock:", options, defaultIndex);
  if(!index.has_value())
    throw std::runtime_error("Canceled.");
  if(index.value() < users.size())
    return users[index.value()];

  auto userName = TerminalUtils::ReadLine("User name: ");
  if(!userName.has_value() || userName->empty())
    throw std::runtime_error("Canceled.");
  isManualUserName = true;
  return userName.value();
}

std::string PairingCommand::ReadPassword(const CommandLine &cmd, const std::string &userName) {
  if(cmd.HasFlag("--password-stdin") && !TerminalUtils::IsInteractive()) {
    std::string password{};
    if(!std::getline(std::cin, password))
      throw std::runtime_error("No password on stdin.");
    if(!password.empty() && password.back() == '\r')
      password.pop_back();
    return password;
  }
  if(!TerminalUtils::IsInteractive())
    throw std::invalid_argument("Missing --password-stdin.");
  auto password = TerminalUtils::ReadPassword(fmt::format("Password for {}: ", userName));
  if(!password.has_value())
    throw std::runtime_error("Canceled.");
  return password.value();
}

PairingMethod PairingCommand::SelectMethod(const CommandLine &cmd) {
  if(auto method = cmd.GetOption("--method")) {
    if(method == "cloud")
      return PairingMethod::CLOUD;
    if(method == "udp")
      return PairingMethod::UDP;
    if(method == "tcp")
      return PairingMethod::TCP;
    if(method == "bt" || method == "bluetooth")
      return PairingMethod::BLUETOOTH;
    if(method == "manual")
      return PairingMethod::MANUAL_UDP;
    throw std::invalid_argument(fmt::format("Unknown method '{}'. Use cloud, udp, tcp, bt or manual.", method.value()));
  }
  if(cmd.GetOption("--bt-address").has_value())
    return PairingMethod::BLUETOOTH;
  if(!TerminalUtils::IsInteractive())
    throw std::invalid_argument("Missing --method.");

  std::vector<std::pair<PairingMethod, std::string>> methods = {
      {PairingMethod::CLOUD, "Cloud (recommended): unlock over the internet, requires a Cloud subscription"},
      {PairingMethod::UDP, "UDP: local network, the phone is found automatically"},
      {PairingMethod::TCP, "TCP: local network, unlock prompts appear faster than with UDP"},
      {PairingMethod::MANUAL_UDP, "Manual UDP: local network, unlock is requested from within the app"},
  };
  if(BluetoothHelper::IsAvailable())
    methods.emplace_back(PairingMethod::BLUETOOTH, "Bluetooth: works without a network connection");
  std::vector<std::string> options{};
  for(const auto &[method, name] : methods)
    options.push_back(name);
  auto index = TerminalUtils::Choose("Pairing method:", options);
  if(!index.has_value())
    throw std::runtime_error("Canceled.");
  return methods[index.value()].first;
}

std::string PairingCommand::SelectBluetoothDevice(const CommandLine &cmd) {
  if(!BluetoothHelper::IsAvailable())
    throw std::runtime_error("Bluetooth is not available.");
  if(auto address = cmd.GetOption("--bt-address"))
    return address.value();
  if(!TerminalUtils::IsInteractive())
    throw std::invalid_argument("Missing --bt-address.");

  std::cout << "Open the Bluetooth settings on your phone so it can be found." << std::endl;
  while(true) {
    std::cout << I18n::Get("pairing_form_bt_scanning") << std::endl;
    BluetoothHelper::StartScan();
    auto devices = BluetoothHelper::ScanDevices();
    BluetoothHelper::StopScan();
    if(devices.empty()) {
      if(!TerminalUtils::Confirm("No devices found. Scan again?"))
        throw std::runtime_error("Canceled.");
      continue;
    }

    std::vector<std::string> options{};
    for(const auto &device : devices)
      options.push_back(fmt::format("{} ({})", device.name, device.address));
    options.emplace_back("Scan again");
    auto index = TerminalUtils::Choose("Phone:", options);
    if(!index.has_value())
      throw std::runtime_error("Canceled.");
    if(index.value() < devices.size())
      return devices[index.value()].address;
  }
}
