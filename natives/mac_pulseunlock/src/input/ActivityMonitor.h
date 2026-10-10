#ifndef PCBU_MAC_ACTIVITYMONITOR_H
#define PCBU_MAC_ACTIVITYMONITOR_H

#include <functional>

#import <AppKit/AppKit.h>

class ActivityMonitor {
public:
  ActivityMonitor(const std::function<void()> &onActivity, bool canReadEventSource);
  ~ActivityMonitor();

  void Start(CFTimeInterval gracePeriod);
  void Stop();

  [[nodiscard]] CFTimeInterval GetSecondsSinceKeyPress() const;
  [[nodiscard]] bool IsCancelChordPressed() const;

private:
  [[nodiscard]] CFTimeInterval GetSecondsSinceUserInput() const;
  static CFTimeInterval GetSecondsSinceHIDInput();

  void Poll();
  void Fire();

  std::function<void()> m_OnActivity{};
  NSTimer *m_PollTimer{};
  id m_WakeObserver{};
  CFAbsoluteTime m_ArmTime{};
  CFTimeInterval m_GracePeriod{};
  bool m_CanReadEventSource{};

  static constexpr NSTimeInterval POLL_INTERVAL = 0.05;
};

#endif
