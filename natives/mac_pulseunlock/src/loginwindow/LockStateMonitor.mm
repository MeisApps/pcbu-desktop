#include "LockStateMonitor.h"

#include <spdlog/spdlog.h>

LockStateMonitor::LockStateMonitor(const std::function<void(bool)> &onLockChanged) {
  m_OnLockChanged = onLockChanged;
}

LockStateMonitor::~LockStateMonitor() {
  Stop();
}

void LockStateMonitor::Start() {
  auto distributedCenter = NSDistributedNotificationCenter.defaultCenter;
  m_DistributedObservers.push_back([distributedCenter addObserverForName:@"com.apple.screenIsLocked"
                                                                  object:nil
                                                                   queue:NSOperationQueue.mainQueue
                                                              usingBlock:^(NSNotification *) {
                                                                spdlog::info("Received screenIsLocked.");
                                                                Evaluate();
                                                              }]);
  m_DistributedObservers.push_back([distributedCenter addObserverForName:@"com.apple.screenIsUnlocked"
                                                                  object:nil
                                                                   queue:NSOperationQueue.mainQueue
                                                              usingBlock:^(NSNotification *) {
                                                                spdlog::info("Received screenIsUnlocked.");
                                                                m_IgnoreSessionLockUntil = CFAbsoluteTimeGetCurrent() + UNLOCK_SETTLE_TIME;
                                                                SetLocked(false);
                                                              }]);
  m_DistributedObservers.push_back([distributedCenter addObserverForName:@"com.apple.screensaver.didstop"
                                                                  object:nil
                                                                   queue:NSOperationQueue.mainQueue
                                                              usingBlock:^(NSNotification *) {
                                                                Evaluate();
                                                              }]);

  auto workspaceCenter = NSWorkspace.sharedWorkspace.notificationCenter;
  for(NSNotificationName name in @[
        NSWorkspaceSessionDidBecomeActiveNotification, NSWorkspaceSessionDidResignActiveNotification, NSWorkspaceDidWakeNotification,
        NSWorkspaceScreensDidWakeNotification
      ]) {
    m_WorkspaceObservers.push_back([workspaceCenter addObserverForName:name
                                                                object:nil
                                                                 queue:NSOperationQueue.mainQueue
                                                            usingBlock:^(NSNotification *) {
                                                              Evaluate();
                                                            }]);
  }

  m_PollTimer = [NSTimer scheduledTimerWithTimeInterval:POLL_INTERVAL
                                                repeats:YES
                                                  block:^(NSTimer *) {
                                                    Evaluate();
                                                  }];
  Evaluate();
}

void LockStateMonitor::Stop() {
  for(id observer : m_DistributedObservers)
    [NSDistributedNotificationCenter.defaultCenter removeObserver:observer];
  m_DistributedObservers.clear();
  for(id observer : m_WorkspaceObservers)
    [NSWorkspace.sharedWorkspace.notificationCenter removeObserver:observer];
  m_WorkspaceObservers.clear();
  [m_PollTimer invalidate];
  m_PollTimer = nil;
}

bool LockStateMonitor::IsLocked() const {
  return m_IsLocked;
}

bool LockStateMonitor::IsSessionLocked() {
  auto session = CGSessionCopyCurrentDictionary();
  if(!session)
    return false;
  auto dict = (__bridge_transfer NSDictionary *)session;
  auto isOnConsole = [dict[(__bridge NSString *)kCGSessionOnConsoleKey] boolValue];
  auto isLocked = [dict[@"CGSSessionScreenIsLocked"] boolValue];
  return isOnConsole && isLocked;
}

void LockStateMonitor::Evaluate() {
  auto isSessionLocked = IsSessionLocked();
  if(!isSessionLocked) {
    SetLocked(false);
    return;
  }
  if(CFAbsoluteTimeGetCurrent() < m_IgnoreSessionLockUntil)
    return;
  SetLocked(true);
}

void LockStateMonitor::SetLocked(bool isLocked) {
  if(m_IsLocked == isLocked)
    return;
  m_IsLocked = isLocked;
  spdlog::info("Lock state changed. (IsLocked={})", isLocked);
  m_OnLockChanged(isLocked);
}
