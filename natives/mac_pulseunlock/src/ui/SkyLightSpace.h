#ifndef PCBU_MAC_SKYLIGHTSPACE_H
#define PCBU_MAC_SKYLIGHTSPACE_H

#include <cstdint>

#import <AppKit/AppKit.h>

class SkyLightSpace {
public:
  bool Init();
  [[nodiscard]] bool IsAvailable() const;

  void Attach(NSWindow *window) const;
  void Detach(NSWindow *window) const;

private:
  bool m_IsAvailable{};
  int32_t m_Connection{};
  uint64_t m_Space{};

  int32_t (*m_MainConnectionID)(){};
  uint64_t (*m_SpaceCreate)(int32_t, int32_t, CFDictionaryRef){};
  int32_t (*m_SpaceSetAbsoluteLevel)(int32_t, uint64_t, int32_t){};
  int32_t (*m_ShowSpaces)(int32_t, CFArrayRef){};
  int32_t (*m_SpaceAddWindowsAndRemoveFromSpaces)(int32_t, uint64_t, CFArrayRef, int32_t){};
  int32_t (*m_RemoveWindowsFromSpaces)(int32_t, CFArrayRef, CFArrayRef){};

  static constexpr const char *FRAMEWORK_PATH = "/System/Library/PrivateFrameworks/SkyLight.framework/SkyLight";
  static constexpr int32_t SPACE_FLAGS = 1;
  static constexpr int32_t SPACE_LEVEL = 400;
  static constexpr int32_t MOVE_FLAGS = 7;
};

#endif
