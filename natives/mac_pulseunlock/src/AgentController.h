#ifndef PCBU_MAC_AGENTCONTROLLER_H
#define PCBU_MAC_AGENTCONTROLLER_H

#include <atomic>
#include <memory>
#include <optional>
#include <string>

#import <AppKit/AppKit.h>

#include "auth/AuthProcess.h"
#include "handler/UnlockState.h"
#include "input/ActivityMonitor.h"
#include "loginwindow/LockStateMonitor.h"
#include "storage/AppSettings.h"
#include "storage/MacAgentStorage.h"
#include "ui/StatusOverlay.h"

enum class AgentMode { LOCK_SCREEN, LOGIN_WINDOW };

enum class AgentState { IDLE, ARMED, RUNNING, INJECTING, FAILED };

class AgentController {
public:
  explicit AgentController(AgentMode mode);

  void Start();
  void Stop();

private:
  void OnLockChanged(bool isLocked);
  void OnActivity();
  void OnRetry();
  void OnCancel();
  void OnTick();
  void OnAuthStatus(uint64_t generation, const std::string &status);
  void OnAuthFinished(uint64_t generation, const AuthResult &result, const std::shared_ptr<std::string> &password);

  void Activate();
  void Deactivate();
  void BeginForUser();
  void WaitForUser();
  void Arm();
  void StartAuth();
  void Inject(uint64_t generation, std::shared_ptr<std::string> password, int attempt);
  void Fail(const std::string &message, bool canAutoRetry);
  void Reset();
  void StopAuth();
  void SetState(AgentState state);
  void TrackSelectedUser();
  void UpdateAnchor();
  void UpdateAgentStatus();

  static void WipePassword(const std::shared_ptr<std::string> &password);
  static UnlockState ParseUnlockState(const std::string &status);
  static std::string GetStateName(AgentState state);

  AgentMode m_Mode{};
  AgentState m_State{AgentState::IDLE};
  PCBUAppStorage m_Settings{};
  std::string m_UserName{};
  bool m_CanAutoRetry{};
  bool m_HasRequestedAccessibility{};
  std::atomic<uint64_t> m_Generation{};
  uint64_t m_TickCount{};
  std::optional<bool> m_HasAccessibility{};

  StatusOverlay m_Overlay;
  LockStateMonitor m_LockMonitor;
  ActivityMonitor m_ActivityMonitor;
  std::unique_ptr<AuthProcess> m_AuthProcess{};
  NSTimer *m_TickTimer{};
  NSTimer *m_StatusTimer{};
  id m_AccessibilityObserver{};
  dispatch_queue_t m_InjectQueue{};

  static constexpr NSTimeInterval TICK_INTERVAL = 0.25;
  static constexpr uint64_t USER_CHECK_TICKS = 4;
  static constexpr NSTimeInterval STATUS_INTERVAL = 1.0;
  static constexpr CFTimeInterval ARM_GRACE_PERIOD = 0.3;
  static constexpr CFTimeInterval RETRY_COOLDOWN = 3.0;
  static constexpr CFTimeInterval TYPING_QUIET_TIME = 1.5;
  static constexpr int TYPING_MAX_ATTEMPTS = 30;
  static constexpr int64_t TYPING_RETRY_DELAY_MS = 100;
  static constexpr int64_t UNLOCK_TIMEOUT_MS = 5000;
};

#endif
