#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

#include <credentialprovider.h>

class CUnlockCredential;
class CUnlockListener {
public:
  CUnlockListener() = default;
  void Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, CUnlockCredential *pCredential, const std::wstring &userDomain);
  void Release();

  void Start(bool ignoreWaitKeyPress = false);
  void Stop();

private:
  struct ListenState {
    std::atomic<bool> isRunning{};
  };

  struct ListenContext {
    std::shared_ptr<ListenState> state{};
    bool ignoreWaitKeyPress{};
    CREDENTIAL_PROVIDER_USAGE_SCENARIO providerUsage{};
    CUnlockCredential *credential{};
    std::wstring userDomain{};
    HMODULE module{};
  };

  static DWORD WINAPI ListenThread(LPVOID param);
  static void Listen(ListenContext &context);

  static std::timed_mutex g_UnlockMutex;
  static constexpr auto UNLOCK_SUCCESS_TIMEOUT = std::chrono::seconds(30);
  static constexpr auto NETWORK_POLL_INTERVAL = std::chrono::milliseconds(250);

  std::shared_ptr<ListenState> m_State{};

  CREDENTIAL_PROVIDER_USAGE_SCENARIO m_ProviderUsage{};
  CUnlockCredential *m_Credential{};
  std::wstring m_UserDomain{};
};
