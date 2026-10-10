#include "ActivityMonitor.h"

#include <algorithm>
#include <limits>

#import <IOKit/IOKitLib.h>

#include <spdlog/spdlog.h>

ActivityMonitor::ActivityMonitor(const std::function<void()> &onActivity, bool canReadEventSource) {
  m_OnActivity = onActivity;
  m_CanReadEventSource = canReadEventSource;
}

ActivityMonitor::~ActivityMonitor() {
  Stop();
}

void ActivityMonitor::Start(CFTimeInterval gracePeriod) {
  Stop();
  m_ArmTime = CFAbsoluteTimeGetCurrent();
  m_GracePeriod = gracePeriod;
  m_PollTimer = [NSTimer scheduledTimerWithTimeInterval:POLL_INTERVAL
                                                repeats:YES
                                                  block:^(NSTimer *) {
                                                    Poll();
                                                  }];
  m_WakeObserver = [NSWorkspace.sharedWorkspace.notificationCenter addObserverForName:NSWorkspaceScreensDidWakeNotification
                                                                               object:nil
                                                                                queue:NSOperationQueue.mainQueue
                                                                           usingBlock:^(NSNotification *) {
                                                                             if(CFAbsoluteTimeGetCurrent() - m_ArmTime >= m_GracePeriod)
                                                                               Fire();
                                                                           }];
}

void ActivityMonitor::Stop() {
  [m_PollTimer invalidate];
  m_PollTimer = nil;
  if(m_WakeObserver) {
    [NSWorkspace.sharedWorkspace.notificationCenter removeObserver:m_WakeObserver];
    m_WakeObserver = nil;
  }
}

CFTimeInterval ActivityMonitor::GetSecondsSinceKeyPress() const {
  if(!m_CanReadEventSource)
    return GetSecondsSinceHIDInput();
  return std::min(CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateHIDSystemState, kCGEventKeyDown),
                  CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateHIDSystemState, kCGEventFlagsChanged));
}

CFTimeInterval ActivityMonitor::GetSecondsSinceUserInput() const {
  if(!m_CanReadEventSource)
    return GetSecondsSinceHIDInput();
  auto result = CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateHIDSystemState, kCGEventKeyDown);
  for(auto type : {kCGEventLeftMouseDown, kCGEventRightMouseDown, kCGEventOtherMouseDown})
    result = std::min(result, CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateHIDSystemState, type));
  return result;
}

CFTimeInterval ActivityMonitor::GetSecondsSinceHIDInput() {
  auto service = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOHIDSystem"));
  if(!service)
    return std::numeric_limits<CFTimeInterval>::max();
  auto idleTime = IORegistryEntryCreateCFProperty(service, CFSTR("HIDIdleTime"), kCFAllocatorDefault, 0);
  IOObjectRelease(service);
  if(!idleTime)
    return std::numeric_limits<CFTimeInterval>::max();
  int64_t idleNs{};
  auto hasValue = CFGetTypeID(idleTime) == CFNumberGetTypeID() && CFNumberGetValue(static_cast<CFNumberRef>(idleTime), kCFNumberSInt64Type, &idleNs);
  CFRelease(idleTime);
  if(!hasValue)
    return std::numeric_limits<CFTimeInterval>::max();
  return static_cast<CFTimeInterval>(idleNs) / NSEC_PER_SEC;
}

bool ActivityMonitor::IsCancelChordPressed() const {
  if(!m_CanReadEventSource)
    return false;
  auto flags = CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState);
  return (flags & kCGEventFlagMaskControl) && (flags & kCGEventFlagMaskAlternate) && !(flags & kCGEventFlagMaskCommand);
}

void ActivityMonitor::Poll() {
  auto now = CFAbsoluteTimeGetCurrent();
  if(now - m_ArmTime < m_GracePeriod)
    return;
  auto lastInputTime = now - GetSecondsSinceUserInput();
  if(lastInputTime > m_ArmTime + m_GracePeriod)
    Fire();
}

void ActivityMonitor::Fire() {
  Stop();
  spdlog::debug("User activity detected.");
  m_OnActivity();
}
