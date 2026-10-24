#ifndef PCBU_MAC_ACTIVITYMONITOR_H
#define PCBU_MAC_ACTIVITYMONITOR_H

#include <functional>
#include <optional>
#include <vector>

#import <AppKit/AppKit.h>
#import <IOKit/IOKitLib.h>

class ActivityMonitor {
public:
  ActivityMonitor(const std::function<void()> &onActivity, bool canReadEventSource);
  ~ActivityMonitor();

  void Start(CFTimeInterval gracePeriod);
  void Stop();

  [[nodiscard]] CFTimeInterval GetSecondsSinceKeyPress() const;
  [[nodiscard]] bool IsCancelChordPressed() const;

private:
  static CFTimeInterval GetTime();
  [[nodiscard]] CFTimeInterval GetSecondsSinceUserInput() const;
  static CFTimeInterval GetSecondsSinceHIDInput();
  [[nodiscard]] bool IsAnyDisplayAwake() const;

  [[nodiscard]] std::optional<bool> IsLidClosed() const;
  void UpdateLidState();
  [[nodiscard]] bool WasLidJustOpened(CFTimeInterval now) const;
  void OnScreenWake();
  [[nodiscard]] bool WasScreenJustWokenByUser(CFTimeInterval now) const;

  void Poll();
  void Fire();

  std::function<void()> m_OnActivity{};
  NSTimer *m_PollTimer{};
  CFTimeInterval m_ArmTime{};
  CFTimeInterval m_GracePeriod{};
  CFTimeInterval m_InputBaseline{};
  bool m_CanReadEventSource{};

  io_service_t m_RootDomain{};
  std::vector<id> m_PowerObservers{};
  bool m_WasLidClosed{};
  CFTimeInterval m_LidOpenedAt{};
  CFTimeInterval m_UserWokeScreenAt{};

  static constexpr NSTimeInterval POLL_INTERVAL = 0.05;
  static constexpr CFTimeInterval WAKE_WINDOW = 5.0;
  static constexpr CFTimeInterval DISPLAY_WAKE_WINDOW = 5.0;
  static constexpr CFTimeInterval INPUT_TIME_TOLERANCE = 0.1;
};

#endif
