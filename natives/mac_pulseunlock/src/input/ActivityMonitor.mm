#include "ActivityMonitor.h"

#include <algorithm>
#include <limits>

#import <QuartzCore/QuartzCore.h>

#include <spdlog/spdlog.h>

ActivityMonitor::ActivityMonitor(const std::function<void()> &onActivity, bool canReadEventSource) {
  m_OnActivity = onActivity;
  m_CanReadEventSource = canReadEventSource;
  m_RootDomain = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPMrootDomain"));

  auto notificationCenter = NSWorkspace.sharedWorkspace.notificationCenter;
  for(NSNotificationName name in @[
        NSWorkspaceWillSleepNotification, NSWorkspaceDidWakeNotification, NSWorkspaceScreensDidSleepNotification, NSWorkspaceScreensDidWakeNotification
      ]) {
    m_PowerObservers.push_back([notificationCenter addObserverForName:name
                                                               object:nil
                                                                queue:NSOperationQueue.mainQueue
                                                           usingBlock:^(NSNotification *notification) {
                                                             if([notification.name isEqualToString:NSWorkspaceScreensDidWakeNotification])
                                                               OnScreenWake();
                                                             UpdateLidState();
                                                           }]);
  }
  UpdateLidState();
}

ActivityMonitor::~ActivityMonitor() {
  Stop();
  for(id observer : m_PowerObservers)
    [NSWorkspace.sharedWorkspace.notificationCenter removeObserver:observer];
  m_PowerObservers.clear();
  if(m_RootDomain)
    IOObjectRelease(m_RootDomain);
}

void ActivityMonitor::Start(CFTimeInterval gracePeriod) {
  Stop();
  m_ArmTime = GetTime();
  m_GracePeriod = gracePeriod;
  m_InputBaseline = m_ArmTime + gracePeriod;
  m_PollTimer = [NSTimer scheduledTimerWithTimeInterval:POLL_INTERVAL
                                                repeats:YES
                                                  block:^(NSTimer *) {
                                                    Poll();
                                                  }];
}

void ActivityMonitor::Stop() {
  [m_PollTimer invalidate];
  m_PollTimer = nil;
}

CFTimeInterval ActivityMonitor::GetSecondsSinceKeyPress() const {
  if(!m_CanReadEventSource)
    return GetSecondsSinceHIDInput();
  return std::min(CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateHIDSystemState, kCGEventKeyDown),
                  CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateHIDSystemState, kCGEventFlagsChanged));
}

CFTimeInterval ActivityMonitor::GetTime() {
  return CACurrentMediaTime();
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

bool ActivityMonitor::IsAnyDisplayAwake() const {
  if(!m_CanReadEventSource)
    return true;
  CGDirectDisplayID displays[16]{};
  uint32_t count{};
  if(CGGetOnlineDisplayList(16, displays, &count) != kCGErrorSuccess)
    return true;
  return std::any_of(displays, displays + count, [](CGDirectDisplayID display) { return !CGDisplayIsAsleep(display); });
}

bool ActivityMonitor::IsCancelChordPressed() const {
  if(!m_CanReadEventSource)
    return false;
  auto flags = CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState);
  return (flags & kCGEventFlagMaskControl) && (flags & kCGEventFlagMaskAlternate) && !(flags & kCGEventFlagMaskCommand);
}

std::optional<bool> ActivityMonitor::IsLidClosed() const {
  if(!m_RootDomain)
    return {};
  auto state = IORegistryEntryCreateCFProperty(m_RootDomain, CFSTR("AppleClamshellState"), kCFAllocatorDefault, 0);
  if(!state)
    return {};
  std::optional<bool> result{};
  if(CFGetTypeID(state) == CFBooleanGetTypeID())
    result = CFBooleanGetValue(static_cast<CFBooleanRef>(state));
  CFRelease(state);
  return result;
}

void ActivityMonitor::UpdateLidState() {
  auto isClosed = IsLidClosed();
  if(!isClosed.has_value())
    return;
  if(isClosed.value()) {
    m_WasLidClosed = true;
    return;
  }
  if(!m_WasLidClosed)
    return;

  m_WasLidClosed = false;
  m_LidOpenedAt = GetTime();
  spdlog::info("Lid opened.");
}

bool ActivityMonitor::WasLidJustOpened(CFTimeInterval now) const {
  return m_LidOpenedAt > 0 && now - m_LidOpenedAt < WAKE_WINDOW;
}

void ActivityMonitor::OnScreenWake() {
  auto secondsSinceHIDInput = GetSecondsSinceHIDInput();
  spdlog::debug("Screen woke. (HIDIdle={:.1f}s, Input={:.1f}s)", secondsSinceHIDInput, GetSecondsSinceUserInput());
  if(secondsSinceHIDInput < DISPLAY_WAKE_WINDOW)
    m_UserWokeScreenAt = GetTime();
}

bool ActivityMonitor::WasScreenJustWokenByUser(CFTimeInterval now) const {
  return m_UserWokeScreenAt > 0 && now - m_UserWokeScreenAt < WAKE_WINDOW;
}

void ActivityMonitor::Poll() {
  auto now = GetTime();
  if(now - m_ArmTime < m_GracePeriod)
    return;

  UpdateLidState();
  if(WasLidJustOpened(now) || WasScreenJustWokenByUser(now)) {
    Fire();
    return;
  }

  auto lastInputTime = now - GetSecondsSinceUserInput();
  if(lastInputTime <= m_InputBaseline + INPUT_TIME_TOLERANCE)
    return;
  if(!IsAnyDisplayAwake()) {
    if(now - lastInputTime < DISPLAY_WAKE_WINDOW)
      return;
    spdlog::info("Input while displays asleep, ignoring.");
    m_InputBaseline = lastInputTime;
    return;
  }
  Fire();
}

void ActivityMonitor::Fire() {
  m_LidOpenedAt = 0;
  m_UserWokeScreenAt = 0;
  Stop();
  spdlog::debug("User activity detected.");
  m_OnActivity();
}
