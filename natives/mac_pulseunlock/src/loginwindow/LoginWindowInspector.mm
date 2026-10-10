#include "LoginWindowInspector.h"

#include <algorithm>
#include <libproc.h>
#include <pwd.h>
#include <set>
#include <string_view>

#import <AppKit/AppKit.h>
#include <spdlog/spdlog.h>

std::optional<CGRect> LoginWindowInspector::GetFocusedPasswordFieldFrame() {
  auto focused = CopyFocusedElement();
  if(!focused)
    return {};
  auto values = CopyAttributes(
      focused, (__bridge CFArrayRef)
                   @[ (__bridge NSString *)kAXSubroleAttribute, (__bridge NSString *)kAXPositionAttribute, (__bridge NSString *)kAXSizeAttribute ]);
  CFRelease(focused);
  if(!values)
    return {};
  std::optional<CGRect> frame{};
  if(ToString(CFArrayGetValueAtIndex(values, 0)) == "AXSecureTextField")
    frame = ToFrame(CFArrayGetValueAtIndex(values, 1), CFArrayGetValueAtIndex(values, 2));
  CFRelease(values);
  return frame;
}

std::optional<CGRect> LoginWindowInspector::GetPasswordFieldFrame() {
  if(auto frame = GetFocusedPasswordFieldFrame())
    return frame;
  return ScanLoginWindow().passwordFieldFrame;
}

bool LoginWindowInspector::IsOtherElementFocused() {
  auto focused = CopyFocusedElement();
  if(!focused)
    return false;
  auto values = CopyAttributes(focused, (__bridge CFArrayRef) @[ (__bridge NSString *)kAXRoleAttribute, (__bridge NSString *)kAXSubroleAttribute ]);
  CFRelease(focused);
  if(!values)
    return false;
  auto role = ToString(CFArrayGetValueAtIndex(values, 0));
  auto subrole = ToString(CFArrayGetValueAtIndex(values, 1));
  CFRelease(values);
  return subrole != "AXSecureTextField" && (role == "AXTextField" || role == "AXTextArea" || role == "AXComboBox");
}

std::string LoginWindowInspector::GetSelectedUser() {
  static const auto users = GetLocalUsers();
  if(users.empty())
    return {};

  auto scan = ScanLoginWindow();
  if(!scan.passwordFieldFrame.has_value())
    return {};
  std::set<std::string> matches{};
  for(const auto &text : scan.texts) {
    for(const auto &user : users) {
      if(text == user.userName || (!user.fullName.empty() && text == user.fullName))
        matches.insert(user.userName);
    }
  }
  if(matches.size() == 1)
    return *matches.begin();
  if(matches.empty() && users.size() == 1)
    return users[0].userName;
  return {};
}

bool LoginWindowInspector::SubmitPassword(const std::string &password) {
  auto field = FindPasswordField();
  if(!field) {
    spdlog::warn("No password field to submit the password to.");
    return false;
  }
  auto error = AXUIElementSetAttributeValue(field.get(), kAXFocusedAttribute, kCFBooleanTrue);
  if(error != kAXErrorSuccess)
    spdlog::warn("Failed focusing password field. (Code={})", static_cast<int>(error));

  auto value = CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(password.data()), static_cast<CFIndex>(password.size()),
                                       kCFStringEncodingUTF8, false);
  error = value ? AXUIElementSetAttributeValue(field.get(), kAXValueAttribute, value) : kAXErrorFailure;
  if(value)
    CFRelease(value);
  if(error != kAXErrorSuccess) {
    spdlog::warn("Failed setting password field value. (Code={})", static_cast<int>(error));
    return false;
  }
  error = AXUIElementPerformAction(field.get(), kAXConfirmAction);
  if(error != kAXErrorSuccess) {
    spdlog::warn("Failed confirming password field. (Code={})", static_cast<int>(error));
    return false;
  }
  return true;
}

AXUIElementRef LoginWindowInspector::CopyFocusedElement() {
  auto systemWide = AXUIElementCreateSystemWide();
  AXUIElementSetMessagingTimeout(systemWide, MESSAGING_TIMEOUT);
  CFTypeRef focused{};
  auto error = AXUIElementCopyAttributeValue(systemWide, kAXFocusedUIElementAttribute, &focused);
  CFRelease(systemWide);
  if(error != kAXErrorSuccess || !focused)
    return nullptr;
  if(CFGetTypeID(focused) != AXUIElementGetTypeID()) {
    CFRelease(focused);
    return nullptr;
  }
  return static_cast<AXUIElementRef>(focused);
}

std::shared_ptr<const __AXUIElement> LoginWindowInspector::FindPasswordField() {
  if(auto focused = CopyFocusedElement()) {
    auto values = CopyAttributes(focused, (__bridge CFArrayRef) @[ (__bridge NSString *)kAXSubroleAttribute ]);
    auto isPasswordField = values && ToString(CFArrayGetValueAtIndex(values, 0)) == "AXSecureTextField";
    if(values)
      CFRelease(values);
    if(isPasswordField)
      return {focused, CFRelease};
    CFRelease(focused);
  }
  return ScanLoginWindow().passwordField;
}

CFArrayRef LoginWindowInspector::CopyAttributes(AXUIElementRef element, CFArrayRef attributes) {
  CFArrayRef values{};
  if(AXUIElementCopyMultipleAttributeValues(element, attributes, 0, &values) != kAXErrorSuccess || !values)
    return nullptr;
  if(CFArrayGetCount(values) != CFArrayGetCount(attributes)) {
    CFRelease(values);
    return nullptr;
  }
  return values;
}

std::string LoginWindowInspector::ToString(CFTypeRef value) {
  if(!value || CFGetTypeID(value) != CFStringGetTypeID())
    return {};
  auto str = (__bridge NSString *)value;
  return str.UTF8String ? str.UTF8String : "";
}

std::optional<CGRect> LoginWindowInspector::ToFrame(CFTypeRef position, CFTypeRef size) {
  if(!position || !size || CFGetTypeID(position) != AXValueGetTypeID() || CFGetTypeID(size) != AXValueGetTypeID())
    return {};
  CGRect frame{};
  if(!AXValueGetValue(static_cast<AXValueRef>(position), kAXValueTypeCGPoint, &frame.origin) ||
     !AXValueGetValue(static_cast<AXValueRef>(size), kAXValueTypeCGSize, &frame.size) || frame.size.width <= 0 || frame.size.height <= 0)
    return {};
  return frame;
}

LoginWindowScan LoginWindowInspector::ScanLoginWindow() {
  auto scan = LoginWindowScan();
  for(auto pid : GetLoginWindowPids()) {
    scan = LoginWindowScan();
    auto app = AXUIElementCreateApplication(pid);
    AXUIElementSetMessagingTimeout(app, MESSAGING_TIMEOUT);
    size_t visited{};
    CollectElements(app, 0, visited, scan);
    CFRelease(app);
    if(scan.passwordFieldFrame.has_value())
      break;
  }
  return scan;
}

std::vector<pid_t> LoginWindowInspector::GetLoginWindowPids() {
  std::vector<pid_t> result{};
  auto addPid = [&result](pid_t pid) {
    if(pid > 0 && std::ranges::find(result, pid) == result.end())
      result.push_back(pid);
  };
  if(auto focused = CopyFocusedElement()) {
    pid_t pid{};
    if(AXUIElementGetPid(focused, &pid) == kAXErrorSuccess)
      addPid(pid);
    CFRelease(focused);
  }
  for(NSRunningApplication *app in [NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.apple.loginwindow"])
    addPid(app.processIdentifier);

  auto numPids = proc_listallpids(nullptr, 0);
  if(numPids <= 0)
    return result;
  std::vector<pid_t> pids(numPids * 2);
  numPids = proc_listallpids(pids.data(), static_cast<int>(pids.size() * sizeof(pid_t)));
  for(int i = 0; i < numPids; i++) {
    char name[2 * MAXCOMLEN + 1]{};
    if(proc_name(pids[i], name, sizeof(name)) > 0 && std::string_view(name) == "loginwindow")
      addPid(pids[i]);
  }
  return result;
}

void LoginWindowInspector::CollectElements(AXUIElementRef element, int depth, size_t &visited, LoginWindowScan &scan) {
  if(depth > MAX_TREE_DEPTH || ++visited > MAX_TREE_ELEMENTS)
    return;
  auto values = CopyAttributes(element, (__bridge CFArrayRef) @[
    (__bridge NSString *)kAXRoleAttribute, (__bridge NSString *)kAXSubroleAttribute, (__bridge NSString *)kAXPositionAttribute,
    (__bridge NSString *)kAXSizeAttribute, (__bridge NSString *)kAXValueAttribute, (__bridge NSString *)kAXTitleAttribute,
    (__bridge NSString *)kAXDescriptionAttribute, (__bridge NSString *)kAXChildrenAttribute
  ]);
  if(!values)
    return;

  auto role = ToString(CFArrayGetValueAtIndex(values, 0));
  if(ToString(CFArrayGetValueAtIndex(values, 1)) == "AXSecureTextField") {
    if(!scan.passwordFieldFrame.has_value())
      scan.passwordFieldFrame = ToFrame(CFArrayGetValueAtIndex(values, 2), CFArrayGetValueAtIndex(values, 3));
    if(!scan.passwordField)
      scan.passwordField = {static_cast<AXUIElementRef>(CFRetain(element)), CFRelease};
    CFRelease(values);
    return;
  }
  if(role == "AXStaticText" || role == "AXTextField" || role == "AXButton" || role == "AXCell") {
    for(CFIndex i = 4; i <= 6; i++) {
      auto text = ToString(CFArrayGetValueAtIndex(values, i));
      if(!text.empty())
        scan.texts.push_back(text);
    }
  }

  auto children = CFArrayGetValueAtIndex(values, 7);
  if(children && CFGetTypeID(children) == CFArrayGetTypeID()) {
    auto array = static_cast<CFArrayRef>(children);
    for(CFIndex i = 0; i < CFArrayGetCount(array); i++) {
      auto child = static_cast<AXUIElementRef>(CFArrayGetValueAtIndex(array, i));
      if(child && CFGetTypeID(child) == AXUIElementGetTypeID())
        CollectElements(child, depth + 1, visited, scan);
    }
  }
  CFRelease(values);
}

std::vector<LocalUser> LoginWindowInspector::GetLocalUsers() {
  std::vector<LocalUser> result{};
  setpwent();
  while(auto entry = getpwent()) {
    if(entry->pw_uid < 501 || !entry->pw_name || !entry->pw_dir)
      continue;
    std::string homeDir = entry->pw_dir;
    std::string shell = entry->pw_shell ? entry->pw_shell : "";
    if(!homeDir.starts_with("/Users/") || shell == "/usr/bin/false" || shell == "/sbin/nologin")
      continue;
    auto user = LocalUser();
    user.userName = entry->pw_name;
    user.fullName = entry->pw_gecos ? entry->pw_gecos : "";
    if(std::ranges::none_of(result, [&user](const LocalUser &u) { return u.userName == user.userName; }))
      result.push_back(user);
  }
  endpwent();
  return result;
}
