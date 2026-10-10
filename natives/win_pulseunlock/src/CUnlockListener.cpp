#include "CUnlockListener.h"

#include "CSampleProvider.h"
#include "handler/UnlockHandler.h"
#include "helpers.h"
#include "platform/NetworkHelper.h"
#include "storage/AppSettings.h"
#include "utils/StringUtils.h"

std::timed_mutex CUnlockListener::g_UnlockMutex{};

void CUnlockListener::Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, CUnlockCredential *pCredential, const std::wstring &userDomain) {
  m_ProviderUsage = cpus;
  m_Credential = pCredential;
  m_UserDomain = userDomain;
}

void CUnlockListener::Release() {
  Stop();
}

void CUnlockListener::Start(bool ignoreWaitKeyPress) {
  if(m_State != nullptr && m_State->isRunning)
    return;

  HMODULE module{};
  if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(&g_hinst), &module)) {
    spdlog::error("Failed to reference module. (Code={})", GetLastError());
    return;
  }
  ListenContext *context{};
  try {
    context = new ListenContext{std::make_shared<ListenState>(), ignoreWaitKeyPress, m_ProviderUsage, m_Credential, m_UserDomain, module};
  } catch(const std::exception &ex) {
    spdlog::error("Failed to allocate listener context: {}", ex.what());
    FreeLibrary(module);
    return;
  }

  context->state->isRunning = true;
  m_Credential->AddRef();
  HANDLE thread = CreateThread(nullptr, 0, &CUnlockListener::ListenThread, context, 0, nullptr);
  if(thread == nullptr) {
    spdlog::error("Failed to start listener thread. (Code={})", GetLastError());
    m_Credential->Release();
    delete context;
    FreeLibrary(module);
    return;
  }
  CloseHandle(thread);
  m_State = context->state;
}

void CUnlockListener::Stop() {
  if(m_State == nullptr)
    return;
  m_State->isRunning = false;
  m_State = nullptr;
}

#define KEY_RANGE 0xA6
void GetAllKeyState(byte *keys, size_t len) {
  for(int i = 0; i < len; i++) {
    if(GetAsyncKeyState(i) < 0)
      keys[i] = 1;
    else
      keys[i] = 0;
  }
}

DWORD WINAPI CUnlockListener::ListenThread(LPVOID param) {
  auto context = static_cast<ListenContext *>(param);
  const auto module = context->module;
  auto hasFailed = false;
  try {
    Listen(*context);
  } catch(const std::exception &ex) {
    spdlog::error("Listener thread failed: {}", ex.what());
    hasFailed = true;
  } catch(...) {
    spdlog::error("Listener thread failed.");
    hasFailed = true;
  }
  if(hasFailed) {
    try {
      context->credential->SetUnlockData(UnlockResult(UnlockState::UNK_ERROR), &context->state->isRunning);
    } catch(...) {
    }
  }
  context->state->isRunning = false;
  context->credential->Release();
  delete context;
  FreeLibraryAndExitThread(module, 0);
}

void CUnlockListener::Listen(ListenContext &context) {
  const auto &isRunning = context.state->isRunning;
  const auto credential = context.credential;
  std::function<void(const std::string &)> printMessage = [credential, state = context.state](const std::string &s) {
    if(state->isRunning)
      credential->UpdateMessage(s);
  };

  // Init
  printMessage(I18n::Get("initializing"));
  const auto userDomainStr = StringUtils::FromWideString(context.userDomain);
  const auto userSplit = StringUtils::Split(userDomainStr, "\\");
  if(userSplit.size() != 2) {
    printMessage(I18n::Get("error_invalid_user"));
    return;
  }

  // Wait
  Sleep(500);
  auto storage = AppSettings::Get();
  auto devices = PairedDevicesStorage::GetDevices();
  const auto waitForNetwork =
      std::ranges::any_of(devices, [](const PairedDevice &device) { return device.pairingMethod != PairingMethod::BLUETOOTH; });
  if(context.providerUsage == CPUS_LOGON || context.providerUsage == CPUS_UNLOCK_WORKSTATION) {
    const bool isUserLoggedOn = IsUserLoggedOn(context.userDomain, 15);

    // Network
    if(waitForNetwork) {
      printMessage(I18n::Get("wait_network"));
      auto nextNetworkCheck = std::chrono::steady_clock::now();
      while(isRunning) {
        if(GetAsyncKeyState(VK_LCONTROL) < 0 && GetAsyncKeyState(VK_LMENU) < 0) {
          credential->SetUnlockData(UnlockResult(UnlockState::CANCELED), &isRunning);
          return;
        }
        const auto now = std::chrono::steady_clock::now();
        if(now >= nextNetworkCheck) {
          if(NetworkHelper::HasLANConnection())
            break;
          nextNetworkCheck = now + NETWORK_POLL_INTERVAL;
        }
        Sleep(10);
      }
    }

    // Unlock behavior
    if(!context.ignoreWaitKeyPress) {
      const bool isUnlock = context.providerUsage == CPUS_UNLOCK_WORKSTATION || (context.providerUsage == CPUS_LOGON && isUserLoggedOn);
      if(storage.unlockBehavior == "key_press" || (storage.unlockBehavior == "key_press_lock_only" && isUnlock)) {
        Sleep(500);
        printMessage(I18n::Get("wait_key_press"));
        byte lastKeys[KEY_RANGE];
        GetAllKeyState(lastKeys, KEY_RANGE);
        while(isRunning) {
          byte keys[KEY_RANGE];
          GetAllKeyState(keys, KEY_RANGE);
          if(memcmp(keys, lastKeys, KEY_RANGE) != 0)
            break;
          Sleep(10);
        }
      } else if(storage.unlockBehavior == "foreground_always" || (storage.unlockBehavior == "foreground_lock_only" && isUnlock)) {
        // HACK: Might not be 100% reliable
        DWORD currentProcessId = GetCurrentProcessId();
        while(isRunning) {
          if(HWND hwndForeground = GetForegroundWindow()) {
            DWORD foregroundProcessId = 0;
            GetWindowThreadProcessId(hwndForeground, &foregroundProcessId);
            if(foregroundProcessId == currentProcessId) {
              break;
            }
          }
          Sleep(100);
        }
      }
    }
  }

  // Unlock
  std::unique_lock unlockLock(g_UnlockMutex, std::defer_lock);
  while(isRunning && !unlockLock.try_lock_for(std::chrono::milliseconds(20))) {
  }
  if(!isRunning)
    return;

  auto handler = UnlockHandler(printMessage);
  auto result = handler.GetResult(userDomainStr, "Windows-Login", {}, &context.state->isRunning);
  unlockLock.unlock();

  const auto sequence = credential->SetUnlockData(result, &isRunning);
  CUnlockCredential::SecureErase(result.password);
  CUnlockCredential::SecureErase(result.passwordKey);
  if(sequence == 0 || result.state != UnlockState::SUCCESS)
    return;

  credential->UpdateProvider();
  const auto deadline = std::chrono::steady_clock::now() + UNLOCK_SUCCESS_TIMEOUT;
  while(credential->IsUnlockPending(sequence) && std::chrono::steady_clock::now() < deadline)
    Sleep(100);
  credential->ExpireUnlockSuccess(sequence);
}
