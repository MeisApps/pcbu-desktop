#include "AccessibilityPermission.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

bool AccessibilityPermission::IsGranted() {
  return AXIsProcessTrusted();
}

void AccessibilityPermission::Request() {
  AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef) @{(__bridge NSString *)kAXTrustedCheckOptionPrompt : @YES});
}

void AccessibilityPermission::OpenSettings() {
  [NSWorkspace.sharedWorkspace openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"]];
}
