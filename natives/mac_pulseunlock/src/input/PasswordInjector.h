#ifndef PCBU_MAC_PASSWORDINJECTOR_H
#define PCBU_MAC_PASSWORDINJECTOR_H

#include <functional>
#include <map>
#include <string>
#include <utility>

#import <ApplicationServices/ApplicationServices.h>

class PasswordInjector {
public:
  PasswordInjector();

  void Type(const std::string &password, const std::function<bool()> &shouldContinue) const;

private:
  static void PostKey(CGEventSourceRef source, CGKeyCode keyCode, CGEventFlags flags, const UniChar *chars, size_t numChars);
  static void PostModifiers(CGEventSourceRef source, CGEventFlags flags, bool isDown);
  static void Post(CGEventRef event);

  static std::map<UniChar, std::pair<CGKeyCode, CGEventFlags>> BuildKeyMap();

  std::map<UniChar, std::pair<CGKeyCode, CGEventFlags>> m_KeyMap{};

  static constexpr useconds_t EVENT_DELAY_US = 500;
};

#endif
