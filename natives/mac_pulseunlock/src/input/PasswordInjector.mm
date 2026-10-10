#include "PasswordInjector.h"

#include <algorithm>
#include <unistd.h>
#include <vector>

#import <Carbon/Carbon.h>
#import <Foundation/Foundation.h>

PasswordInjector::PasswordInjector() {
  m_KeyMap = BuildKeyMap();
}

void PasswordInjector::Type(const std::string &password, const std::function<bool()> &shouldContinue) const {
  auto source = CGEventSourceCreate(kCGEventSourceStatePrivate);

  CGKeyCode selectAllKey = kVK_ANSI_A;
  if(auto it = m_KeyMap.find('a'); it != m_KeyMap.end() && it->second.second == 0)
    selectAllKey = it->second.first;
  PostModifiers(source, kCGEventFlagMaskCommand, true);
  PostKey(source, selectAllKey, kCGEventFlagMaskCommand, nullptr, 0);
  PostModifiers(source, kCGEventFlagMaskCommand, false);
  PostKey(source, kVK_Delete, 0, nullptr, 0);

  auto str = [[NSString alloc] initWithBytes:password.data() length:password.size() encoding:NSUTF8StringEncoding];
  NSUInteger index = 0;
  auto isCanceled = false;
  while(index < str.length && !isCanceled) {
    auto range = [str rangeOfComposedCharacterSequenceAtIndex:index];
    std::vector<UniChar> chars(range.length);
    [str getCharacters:chars.data() range:range];
    index = NSMaxRange(range);

    auto it = chars.size() == 1 ? m_KeyMap.find(chars[0]) : m_KeyMap.end();
    if(it != m_KeyMap.end()) {
      auto [keyCode, flags] = it->second;
      PostModifiers(source, flags, true);
      PostKey(source, keyCode, flags, chars.data(), chars.size());
      PostModifiers(source, flags, false);
    } else {
      PostKey(source, 0, 0, chars.data(), chars.size());
    }
    std::fill(chars.begin(), chars.end(), 0);
    isCanceled = !shouldContinue();
  }
  if(!isCanceled)
    PostKey(source, kVK_Return, 0, nullptr, 0);
  if(source)
    CFRelease(source);
}

void PasswordInjector::PostKey(CGEventSourceRef source, CGKeyCode keyCode, CGEventFlags flags, const UniChar *chars, size_t numChars) {
  for(auto isDown : {true, false}) {
    auto event = CGEventCreateKeyboardEvent(source, keyCode, isDown);
    if(!event)
      continue;
    CGEventSetFlags(event, flags);
    if(chars && numChars > 0)
      CGEventKeyboardSetUnicodeString(event, numChars, chars);
    Post(event);
  }
}

void PasswordInjector::PostModifiers(CGEventSourceRef source, CGEventFlags flags, bool isDown) {
  const std::pair<CGEventFlags, CGKeyCode> modifierKeys[] = {
      {kCGEventFlagMaskCommand, kVK_Command},
      {kCGEventFlagMaskShift, kVK_Shift},
      {kCGEventFlagMaskAlternate, kVK_Option},
  };
  CGEventFlags currentFlags = isDown ? 0 : flags;
  for(const auto &[flag, keyCode] : modifierKeys) {
    if(!(flags & flag))
      continue;
    currentFlags = isDown ? (currentFlags | flag) : (currentFlags & ~flag);
    auto event = CGEventCreateKeyboardEvent(source, keyCode, isDown);
    if(!event)
      continue;
    CGEventSetType(event, kCGEventFlagsChanged);
    CGEventSetFlags(event, currentFlags);
    Post(event);
  }
}

void PasswordInjector::Post(CGEventRef event) {
  CGEventPost(kCGHIDEventTap, event);
  CFRelease(event);
  usleep(EVENT_DELAY_US);
}

std::map<UniChar, std::pair<CGKeyCode, CGEventFlags>> PasswordInjector::BuildKeyMap() {
  std::map<UniChar, std::pair<CGKeyCode, CGEventFlags>> result{};
  auto inputSource = TISCopyCurrentKeyboardLayoutInputSource();
  auto layoutData = inputSource ? static_cast<CFDataRef>(TISGetInputSourceProperty(inputSource, kTISPropertyUnicodeKeyLayoutData)) : nullptr;
  if(!layoutData) {
    if(inputSource)
      CFRelease(inputSource);
    inputSource = TISCopyCurrentASCIICapableKeyboardLayoutInputSource();
    layoutData = inputSource ? static_cast<CFDataRef>(TISGetInputSourceProperty(inputSource, kTISPropertyUnicodeKeyLayoutData)) : nullptr;
  }
  if(!layoutData) {
    if(inputSource)
      CFRelease(inputSource);
    return result;
  }

  auto layout = reinterpret_cast<const UCKeyboardLayout *>(CFDataGetBytePtr(layoutData));
  const std::pair<UInt32, CGEventFlags> modifiers[] = {
      {0, 0},
      {(shiftKey >> 8) & 0xFF, kCGEventFlagMaskShift},
      {(optionKey >> 8) & 0xFF, kCGEventFlagMaskAlternate},
      {((shiftKey | optionKey) >> 8) & 0xFF, kCGEventFlagMaskShift | kCGEventFlagMaskAlternate},
  };
  for(const auto &[modifierState, flags] : modifiers) {
    for(CGKeyCode keyCode = 0; keyCode < 128; keyCode++) {
      if(keyCode >= kVK_ANSI_KeypadDecimal && keyCode <= kVK_ANSI_Keypad9)
        continue;
      UInt32 deadKeyState = 0;
      UniChar chars[4]{};
      UniCharCount length = 0;
      auto status = UCKeyTranslate(layout, keyCode, kUCKeyActionDown, modifierState, LMGetKbdType(), kUCKeyTranslateNoDeadKeysBit, &deadKeyState, 4,
                                   &length, chars);
      if(status != noErr || length != 1 || deadKeyState != 0 || chars[0] < 0x20 || chars[0] == 0x7F)
        continue;
      result.try_emplace(chars[0], keyCode, flags);
    }
  }
  CFRelease(inputSource);
  return result;
}
