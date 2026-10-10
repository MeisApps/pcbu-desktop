#ifndef PCBU_MAC_AUTHPROCESS_H
#define PCBU_MAC_AUTHPROCESS_H

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

struct AuthResult {
  int exitCode{-1};
  std::string lastStatus{};
  std::string password{};
  bool isCanceled{};
};

class AuthProcess {
public:
  AuthProcess(const std::string &userName, const std::function<void(const std::string &)> &onStatus,
              const std::function<void(AuthResult)> &onFinished);
  ~AuthProcess();

  void Cancel();

private:
  void Run(const std::string &userName);
  bool WaitForNetwork();

  std::function<void(const std::string &)> m_OnStatus{};
  std::function<void(AuthResult)> m_OnFinished{};
  std::thread m_Thread{};
  std::mutex m_CancelMutex{};
  int m_CancelFd{-1};
  std::atomic<bool> m_IsCanceled{};

  static constexpr auto PCBU_AUTH_PATH = "/usr/local/sbin/pcbu_auth";
  static constexpr auto CANCEL_PIPE_ARG = "--cancel-pipe";
  static constexpr int PASSWORD_PIPE = 3;
  static constexpr int CANCEL_PIPE = 4;
};

#endif
