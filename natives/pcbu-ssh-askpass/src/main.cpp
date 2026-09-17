#include <pwd.h>
#include <unistd.h>

#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include "RootGuard.h"
#include "SshPrompt.h"
#include "TtyPrompt.h"
#include "handler/UnlockHandler.h"
#include "platform/PlatformHelper.h"
#include "storage/LoggingSystem.h"
#include "utils/CryptUtils.h"
#include "utils/I18n.h"

int runMain(int argc, char *argv[], const std::string &userName, const std::string &homeDir) {
  auto rawPrompt = argc >= 2 ? std::string(argv[1]) : std::string();
  auto prompt = SshPrompt::Parse(rawPrompt);
  if(prompt.kind == SshPrompt::Kind::Unknown)
    return TtyPrompt::ReadPassword(rawPrompt);

  auto devices = std::vector<PairedDevice>();
  {
    auto root = RootGuard();
    devices = PairedDevicesStorage::GetDevicesForUser(userName);
  }
  auto deviceIds = devices | std::views::filter([&](const PairedDevice &device) { return prompt.FindMatch(device, homeDir).has_value(); }) |
                   std::views::transform(&PairedDevice::id) | std::ranges::to<std::vector>();
  if(deviceIds.empty())
    return TtyPrompt::ReadPassword(rawPrompt);

  auto handler = UnlockHandler([](const std::string &s) { fmt::print(stderr, "{}\n", s); });
  auto unlock = [&]() -> std::optional<std::string> {
    auto result = UnlockResult();
    auto isLoginValid = false;
    {
      auto root = RootGuard();
      result = handler.GetResult(userName, prompt.GetDisplayName(), deviceIds, nullptr);
      if(result.state == UnlockState::SUCCESS && userName == result.device.userName)
        isLoginValid = PlatformHelper::CheckLogin(userName, result.password).result == PlatformLoginResult::SUCCESS;
    }
    if(result.state != UnlockState::SUCCESS)
      return {};
    if(!isLoginValid) {
      fmt::print(stderr, "{}\n", I18n::Get("error_password"));
      return {};
    }
    auto server = prompt.FindMatch(result.device, homeDir);
    if(!server) {
      fmt::print(stderr, "{}\n", I18n::Get("error_ssh_wrong_device"));
      return {};
    }
    auto sshPw = CryptUtils::DecryptAES(server->passwordEnc, result.passwordKey);
    if(!sshPw)
      fmt::print(stderr, "{}\n", I18n::Get("error_password_decrypt"));
    return sshPw;
  };

  auto sshPw = unlock();
  if(!sshPw)
    return TtyPrompt::ReadPassword(rawPrompt);
  fmt::print("{}\n", *sshPw);
  return 0;
}

int main(int argc, char *argv[]) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  auto pw = getpwuid(getuid());
  if(!pw) {
    fmt::print(stderr, "Failed to resolve user.\n");
    return -1;
  }
  std::string userName = pw->pw_name;
  std::string homeDir = pw->pw_dir ? pw->pw_dir : "";

  if(geteuid() != 0) {
    fmt::print(stderr, "Missing root privileges.\n");
    return -1;
  }
  if(!RootGuard::DropPrivileges()) {
    fmt::print(stderr, "Failed to drop root privileges.\n");
    return -1;
  }

  {
    auto root = RootGuard();
    LoggingSystem::Init("module", false);
  }
  auto result = runMain(argc, argv, userName, homeDir);
  LoggingSystem::Destroy();
  return result;
}
