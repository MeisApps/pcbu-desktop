#include "AgentController.h"

#include <algorithm>
#include <pwd.h>
#include <unistd.h>

#include <spdlog/spdlog.h>

#include "input/PasswordInjector.h"
#include "loginwindow/AccessibilityPermission.h"
#include "loginwindow/LoginWindowInspector.h"
#include "utils/I18n.h"

AgentController::AgentController(AgentMode mode)
    : m_Overlay([this]() { OnRetry(); }, [this]() { OnCancel(); }), m_LockMonitor([this](bool isLocked) { OnLockChanged(isLocked); }),
      m_ActivityMonitor([this]() { OnActivity(); }, mode == AgentMode::LOCK_SCREEN) {
  m_Mode = mode;
  m_InjectQueue = dispatch_queue_create("com.meisapps.PulseUnlock.Agent.Inject", DISPATCH_QUEUE_SERIAL);
}

void AgentController::Start() {
  if(m_Mode == AgentMode::LOGIN_WINDOW) {
    Activate();
    return;
  }

  UpdateAgentStatus();
  m_StatusTimer = [NSTimer scheduledTimerWithTimeInterval:STATUS_INTERVAL
                                                  repeats:YES
                                                    block:^(NSTimer *) {
                                                      UpdateAgentStatus();
                                                    }];
  m_StatusTimer.tolerance = STATUS_INTERVAL / 2;
  auto notificationName = [NSString stringWithUTF8String:std::string(MacAgentStorage::ACCESSIBILITY_REQUEST_NOTIFICATION).c_str()];
  m_AccessibilityObserver = [NSDistributedNotificationCenter.defaultCenter addObserverForName:notificationName
                                                                                       object:nil
                                                                                        queue:NSOperationQueue.mainQueue
                                                                                   usingBlock:^(NSNotification *) {
                                                                                     if(m_LockMonitor.IsLocked())
                                                                                       return;
                                                                                     if(m_HasRequestedAccessibility) {
                                                                                       AccessibilityPermission::OpenSettings();
                                                                                       return;
                                                                                     }
                                                                                     spdlog::info("Requesting accessibility access.");
                                                                                     m_HasRequestedAccessibility = true;
                                                                                     AccessibilityPermission::Request();
                                                                                   }];
  m_LockMonitor.Start();
}

void AgentController::Stop() {
  Deactivate();
  m_LockMonitor.Stop();
  [m_StatusTimer invalidate];
  m_StatusTimer = nil;
  if(m_AccessibilityObserver) {
    [NSDistributedNotificationCenter.defaultCenter removeObserver:m_AccessibilityObserver];
    m_AccessibilityObserver = nil;
  }
}

void AgentController::OnLockChanged(bool isLocked) {
  if(isLocked) {
    Activate();
    return;
  }
  if(m_State == AgentState::INJECTING)
    spdlog::info("Unlock result: {}", I18n::Get("unlock_success"));
  Deactivate();
}

void AgentController::OnActivity() {
  if(m_State == AgentState::ARMED || (m_State == AgentState::FAILED && m_CanAutoRetry))
    StartAuth();
}

void AgentController::OnRetry() {
  if(m_State == AgentState::FAILED || m_State == AgentState::ARMED)
    StartAuth();
}

void AgentController::OnCancel() {
  if(m_State != AgentState::RUNNING && m_State != AgentState::ARMED)
    return;
  spdlog::info("Unlock canceled by user.");
  Fail(I18n::Get("unlock_canceled"), false);
}

void AgentController::OnTick() {
  m_TickCount++;
  if(m_State == AgentState::RUNNING && m_ActivityMonitor.IsCancelChordPressed())
    OnCancel();
  if(m_Mode == AgentMode::LOGIN_WINDOW && m_TickCount % USER_CHECK_TICKS == 0)
    TrackSelectedUser();
  if(m_State != AgentState::IDLE)
    UpdateAnchor();
}

void AgentController::OnAuthStatus(uint64_t generation, const std::string &status) {
  if(generation != m_Generation || m_State != AgentState::RUNNING)
    return;
  m_Overlay.Show(status, ParseUnlockState(status) == UnlockState::SUCCESS ? OverlayStyle::SUCCESS : OverlayStyle::RUNNING);
}

void AgentController::OnAuthFinished(uint64_t generation, const AuthResult &result, const std::shared_ptr<std::string> &password) {
  if(generation != m_Generation || m_State != AgentState::RUNNING || result.isCanceled) {
    WipePassword(password);
    return;
  }
  StopAuth();

  if(result.exitCode == 0 && !password->empty()) {
    Inject(generation, password, 0);
    return;
  }
  WipePassword(password);
  if(result.exitCode == 0) {
    spdlog::error("pcbu_auth succeeded without returning a password.");
    Fail(I18n::Get("error_unknown"), false);
    return;
  }
  auto message = result.lastStatus;
  if(message.empty())
    message = I18n::Get(result.exitCode == 1 ? "unlock_canceled" : "error_unknown");
  Fail(message, result.exitCode != 1 && ParseUnlockState(message) != UnlockState::CLOUD_RATE_LIMITED);
}

void AgentController::Activate() {
  AppSettings::InvalidateCache();
  m_Settings = AppSettings::Get();
  if(!m_Settings.macLoginScreen) {
    spdlog::info("Unlock disabled in settings.");
    return;
  }

  [m_TickTimer invalidate];
  m_TickTimer = [NSTimer scheduledTimerWithTimeInterval:TICK_INTERVAL
                                                repeats:YES
                                                  block:^(NSTimer *) {
                                                    OnTick();
                                                  }];
  UpdateAnchor();
  if(m_Mode == AgentMode::LOCK_SCREEN) {
    auto pw = getpwuid(getuid());
    m_UserName = pw && pw->pw_name ? pw->pw_name : "";
  } else {
    m_UserName = LoginWindowInspector::GetSelectedUser();
  }
  if(m_UserName.empty()) {
    WaitForUser();
    return;
  }
  BeginForUser();
}

void AgentController::Deactivate() {
  Reset();
  [m_TickTimer invalidate];
  m_TickTimer = nil;
}

void AgentController::BeginForUser() {
  spdlog::info("Starting unlock flow. (User={}, Behavior={})", m_UserName, m_Settings.unlockBehavior);
  auto behavior = m_Settings.unlockBehavior;
  auto waitForKey = m_Mode == AgentMode::LOCK_SCREEN ? behavior != "none" : behavior == "key_press";
  if(waitForKey)
    Arm();
  else
    StartAuth();
}

void AgentController::WaitForUser() {
  StopAuth();
  m_ActivityMonitor.Stop();
  m_Generation++;
  m_UserName = {};
  SetState(AgentState::ARMED);
  m_Overlay.Show(I18n::Get("wait_select_user"), OverlayStyle::WAITING);
}

void AgentController::Arm() {
  StopAuth();
  m_Generation++;
  SetState(AgentState::ARMED);
  m_Overlay.Show(I18n::Get("wait_key_press"), OverlayStyle::WAITING);
  m_ActivityMonitor.Start(ARM_GRACE_PERIOD);
}

void AgentController::StartAuth() {
  if(m_UserName.empty())
    return;
  StopAuth();
  m_ActivityMonitor.Stop();
  auto generation = ++m_Generation;
  SetState(AgentState::RUNNING);
  m_Overlay.Show(I18n::Get("initializing"), OverlayStyle::RUNNING);

  m_AuthProcess = std::make_unique<AuthProcess>(
      m_UserName,
      [this, generation](const std::string &status) {
        auto text = status;
        dispatch_async(dispatch_get_main_queue(), ^{
          OnAuthStatus(generation, text);
        });
      },
      [this, generation](AuthResult result) {
        auto password = std::make_shared<std::string>(std::move(result.password));
        result.password.clear();
        dispatch_async(dispatch_get_main_queue(), ^{
          OnAuthFinished(generation, result, password);
        });
      });
}

void AgentController::Inject(uint64_t generation, std::shared_ptr<std::string> password, int attempt) {
  if(generation != m_Generation) {
    WipePassword(password);
    return;
  }
  if(!AccessibilityPermission::IsGranted()) {
    WipePassword(password);
    spdlog::error("Accessibility access missing, cannot type password.");
    Fail(I18n::Get("error_accessibility_missing"), false);
    return;
  }
  if(m_Mode == AgentMode::LOCK_SCREEN && (!m_LockMonitor.IsLocked() || !LockStateMonitor::IsSessionLocked())) {
    WipePassword(password);
    spdlog::info("Screen no longer locked, skipping password injection.");
    Reset();
    return;
  }
  if(m_Mode == AgentMode::LOGIN_WINDOW && LoginWindowInspector::GetSelectedUser() != m_UserName) {
    WipePassword(password);
    spdlog::warn("Selected user changed, skipping password injection.");
    Fail(I18n::Get("wait_select_user"), false);
    return;
  }
  if(LoginWindowInspector::IsOtherElementFocused()) {
    WipePassword(password);
    spdlog::warn("Focused element is not a password field, skipping password injection.");
    Fail(I18n::Get("error_password_field"), false);
    return;
  }
  if(m_ActivityMonitor.GetSecondsSinceKeyPress() < TYPING_QUIET_TIME) {
    if(attempt >= TYPING_MAX_ATTEMPTS) {
      WipePassword(password);
      spdlog::warn("User is typing, skipping password injection.");
      Fail(I18n::Get("error_typing_detected"), false);
      return;
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, TYPING_RETRY_DELAY_MS * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
      Inject(generation, password, attempt + 1);
    });
    return;
  }

  SetState(AgentState::INJECTING);
  m_Overlay.Show(I18n::Get("unlock_success"), OverlayStyle::SUCCESS);
  auto injector = m_Mode == AgentMode::LOCK_SCREEN ? std::make_shared<PasswordInjector>() : nullptr;
  dispatch_async(m_InjectQueue, ^{
    if(injector) {
      injector->Type(*password, [this, generation]() { return m_Generation.load() == generation; });
    } else if(!LoginWindowInspector::SubmitPassword(*password)) {
      WipePassword(password);
      dispatch_async(dispatch_get_main_queue(), ^{
        if(generation == m_Generation && m_State == AgentState::INJECTING)
          Fail(I18n::Get("error_password_field"), false);
      });
      return;
    }
    WipePassword(password);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, UNLOCK_TIMEOUT_MS * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
      if(generation != m_Generation || m_State != AgentState::INJECTING)
        return;
      if(m_Mode == AgentMode::LOGIN_WINDOW && !LoginWindowInspector::GetPasswordFieldFrame().has_value()) {
        spdlog::info("Password field gone after typing, logging in.");
        Reset();
        return;
      }
      spdlog::warn("No unlock after typing the password.");
      Fail(I18n::Get("error_password"), false);
    });
  });
}

void AgentController::Fail(const std::string &message, bool canAutoRetry) {
  StopAuth();
  m_ActivityMonitor.Stop();
  m_Generation++;
  m_CanAutoRetry = canAutoRetry;
  SetState(AgentState::FAILED);
  m_Overlay.Show(message, ParseUnlockState(message) == UnlockState::CANCELED ? OverlayStyle::CANCELED : OverlayStyle::ERROR);
  spdlog::info("Unlock result: {}", message);
  if(canAutoRetry)
    m_ActivityMonitor.Start(RETRY_COOLDOWN);
}

void AgentController::Reset() {
  m_Generation++;
  StopAuth();
  m_ActivityMonitor.Stop();
  m_Overlay.Hide();
  SetState(AgentState::IDLE);
}

void AgentController::StopAuth() {
  if(!m_AuthProcess)
    return;
  auto process = m_AuthProcess.release();
  process->Cancel();
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
    delete process;
  });
}

void AgentController::SetState(AgentState state) {
  if(m_State == state)
    return;
  spdlog::info("Session state: {} -> {}", GetStateName(m_State), GetStateName(state));
  m_State = state;
}

void AgentController::TrackSelectedUser() {
  if(m_State == AgentState::IDLE || m_State == AgentState::INJECTING)
    return;
  auto selectedUser = LoginWindowInspector::GetSelectedUser();
  if(selectedUser == m_UserName)
    return;
  spdlog::info("Selected user changed. (From={}, To={})", m_UserName, selectedUser);
  if(selectedUser.empty()) {
    WaitForUser();
    return;
  }
  m_UserName = selectedUser;
  BeginForUser();
}

void AgentController::UpdateAnchor() {
  auto fieldFrame = m_Overlay.HasAnchor() ? LoginWindowInspector::GetFocusedPasswordFieldFrame() : LoginWindowInspector::GetPasswordFieldFrame();
  if(fieldFrame.has_value())
    m_Overlay.SetAnchor(*fieldFrame);
}

void AgentController::UpdateAgentStatus() {
  if(m_Mode != AgentMode::LOCK_SCREEN)
    return;
  auto hasAccessibility = AccessibilityPermission::IsGranted();
  if(m_HasAccessibility == hasAccessibility)
    return;
  m_HasAccessibility = hasAccessibility;
  spdlog::info("Agent status changed. (HasAccessibility={})", hasAccessibility);
  MacAgentStorage::SetAccessibility(hasAccessibility);
}

void AgentController::WipePassword(const std::shared_ptr<std::string> &password) {
  std::fill(password->begin(), password->end(), '\0');
  password->clear();
}

UnlockState AgentController::ParseUnlockState(const std::string &status) {
  for(auto state = static_cast<int>(UnlockState::SUCCESS); state <= static_cast<int>(UnlockState::UNK_ERROR); state++) {
    if(status == UnlockStateUtils::ToString(static_cast<UnlockState>(state)))
      return static_cast<UnlockState>(state);
  }
  return UnlockState::UNKNOWN;
}

std::string AgentController::GetStateName(AgentState state) {
  switch(state) {
    case AgentState::IDLE:
      return "IDLE";
    case AgentState::ARMED:
      return "ARMED";
    case AgentState::RUNNING:
      return "RUNNING";
    case AgentState::INJECTING:
      return "INJECTING";
    case AgentState::FAILED:
      return "FAILED";
  }
  return "UNKNOWN";
}
