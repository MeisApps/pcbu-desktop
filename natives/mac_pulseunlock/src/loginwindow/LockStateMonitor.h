#ifndef PCBU_MAC_LOCKSTATEMONITOR_H
#define PCBU_MAC_LOCKSTATEMONITOR_H

#include <functional>
#include <vector>

#import <AppKit/AppKit.h>

class LockStateMonitor {
public:
  explicit LockStateMonitor(const std::function<void(bool)> &onLockChanged);
  ~LockStateMonitor();

  void Start();
  void Stop();

  [[nodiscard]] bool IsLocked() const;
  static bool IsSessionLocked();

private:
  void Evaluate();
  void SetLocked(bool isLocked);

  std::function<void(bool)> m_OnLockChanged{};
  std::vector<id> m_DistributedObservers{};
  std::vector<id> m_WorkspaceObservers{};
  NSTimer *m_PollTimer{};
  CFAbsoluteTime m_IgnoreSessionLockUntil{};
  bool m_IsLocked{};

  static constexpr NSTimeInterval POLL_INTERVAL = 0.5;
  static constexpr CFAbsoluteTime UNLOCK_SETTLE_TIME = 3.0;
};

#endif
