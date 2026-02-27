#include <unistd.h>

#include "handler/UnlockHandler.h"
#include "platform/PlatformHelper.h"
#include "storage/AppSettings.h"
#include "storage/LoggingSystem.h"
#include "utils/StringUtils.h"

#ifdef LINUX
#include "shell/Shell.h"
#elif APPLE
#include <sys/sysctl.h>
#endif

constexpr int PASSWORD_PIPE = 3;

std::string GetServiceName() {
  auto fallback = fmt::format("PID {}", getppid());
#if defined(LINUX) || defined(APPLE)
  auto skipNum = 0;
#ifdef LINUX
  auto cmdline = Shell::ReadBytes(fmt::format("/proc/{}/cmdline", getppid()));
#elif APPLE
  int mib[] = {CTL_KERN, KERN_PROCARGS2, getppid()};
  size_t size{};
  if(sysctl(mib, 3, nullptr, &size, nullptr, 0) == -1)
    return fallback;
  std::vector<char> cmdline(size);
  if(sysctl(mib, 3, cmdline.data(), &size, nullptr, 0) == -1)
    return fallback;
  skipNum = 2;
#endif
  if(cmdline.empty())
    return fallback;
  auto i = 0;
  auto start = reinterpret_cast<const char *>(cmdline.data());
  auto end = start + cmdline.size();
  std::ostringstream result{};
  while(start < end) {
    std::string_view str(start);
    if(!str.empty()) {
      if(i > skipNum)
        result << " ";
      if(i > skipNum - 1)
        result << str;
      i++;
    }
    start += str.size() + 1;
  }
  auto resultStr = result.str();
  return resultStr.empty() ? fallback : StringUtils::Truncate(resultStr, 256);
#endif
  return fallback;
}

int runMain(int argc, char *argv[]) {
  if(argc != 2) {
    printf("Invalid parameters.\n");
    return -1;
  }

  std::string userName = argv[1];
  auto handler = UnlockHandler([](const std::string &s) { printf("%s\n", s.c_str()); });
  auto result = handler.GetResult(userName, GetServiceName());
  if(result.state == UnlockState::SUCCESS) {
    if(userName == result.device.userName && PlatformHelper::CheckLogin(userName, result.password) == PlatformLoginResult::SUCCESS) {
      if(AppSettings::Get().unixSetPasswordPAM) {
        size_t bytesWritten{};
        while(bytesWritten < result.password.size()) {
          auto count = write(PASSWORD_PIPE, result.password.data() + bytesWritten, result.password.size() - bytesWritten);
          if(count == -1)
            break;
          bytesWritten += count;
        }
      }
      return 0;
    }

    auto errorMsg = I18n::Get("error_password");
    printf("%s\n", errorMsg.c_str());
    return 1;
  }
  if(result.state == UnlockState::CANCELED) {
    return 1;
  }
  return -1;
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
