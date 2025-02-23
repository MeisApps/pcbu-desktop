#include <unistd.h>

#include "handler/UnlockHandler.h"
#include "platform/PlatformHelper.h"
#include "shell/Shell.h"
#include "storage/LoggingSystem.h"
#include "utils/StringUtils.h"

std::string GetServiceName() {
  const auto fallback = fmt::format("PID {}", getppid());
#ifdef LINUX
  auto cmdline = Shell::ReadBytes(fmt::format("/proc/{}/cmdline", getppid()));
  if(cmdline.empty())
    return fallback;
  auto first = true;
  auto start = reinterpret_cast<const char *>(cmdline.data());
  auto end = start + cmdline.size();
  std::ostringstream result{};
  while(start < end) {
    std::string_view str(start);
    if(!str.empty()) {
      if(!first)
        result << " ";
      result << str;
      first = false;
    }
    start += str.size() + 1;
  }
  return StringUtils::Truncate(result.str(), 256);
#else
  return fallback; // ToDo: macOS
#endif
}

int runMain(int argc, char *argv[]) {
  if(argc != 2) {
    printf("Invalid parameters.\n");
    return -1;
  }
  auto userName = argv[1];
  std::function<void(const std::string &)> printMessage = [](const std::string &s) { printf("%s\n", s.c_str()); };
  auto handler = UnlockHandler(printMessage);
  auto result = handler.GetResult(userName, GetServiceName());
  if(result.state == UnlockState::SUCCESS) {
    if(strcmp(userName, result.device.userName.c_str()) == 0) {
      if(PlatformHelper::CheckLogin(userName, result.password) == PlatformLoginResult::SUCCESS) {
        // pam_set_item(pamh, PAM_AUTHTOK, result.additionalData.c_str()); ToDo
        return 0;
      }
    }

    auto errorMsg = I18n::Get("error_password");
    printf("%s\n", errorMsg.c_str());
    return 1;
  } else if(result.state == UnlockState::CANCELED) {
    return 1;
  } else if(result.state == UnlockState::TIMEOUT || result.state == UnlockState::CONNECT_ERROR) {
    return -1;
  }
  return 1;
}

int main(int argc, char *argv[]) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if(setuid(0) != 0) {
    printf("setuid(0) failed.\n");
    return -1;
  }
  LoggingSystem::Init("module", false);
  auto result = runMain(argc, argv);
  LoggingSystem::Destroy();
  return result;
}
