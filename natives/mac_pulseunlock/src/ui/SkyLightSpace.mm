#include "SkyLightSpace.h"

#include <dlfcn.h>

#include <spdlog/spdlog.h>

bool SkyLightSpace::Init() {
  if(m_IsAvailable)
    return true;

  auto handle = dlopen(FRAMEWORK_PATH, RTLD_NOW);
  if(!handle) {
    spdlog::warn("SkyLight unavailable: {}", dlerror());
    return false;
  }
  m_MainConnectionID = reinterpret_cast<decltype(m_MainConnectionID)>(dlsym(handle, "SLSMainConnectionID"));
  m_SpaceCreate = reinterpret_cast<decltype(m_SpaceCreate)>(dlsym(handle, "SLSSpaceCreate"));
  m_SpaceSetAbsoluteLevel = reinterpret_cast<decltype(m_SpaceSetAbsoluteLevel)>(dlsym(handle, "SLSSpaceSetAbsoluteLevel"));
  m_ShowSpaces = reinterpret_cast<decltype(m_ShowSpaces)>(dlsym(handle, "SLSShowSpaces"));
  m_SpaceAddWindowsAndRemoveFromSpaces =
      reinterpret_cast<decltype(m_SpaceAddWindowsAndRemoveFromSpaces)>(dlsym(handle, "SLSSpaceAddWindowsAndRemoveFromSpaces"));
  m_RemoveWindowsFromSpaces = reinterpret_cast<decltype(m_RemoveWindowsFromSpaces)>(dlsym(handle, "SLSRemoveWindowsFromSpaces"));
  if(!m_MainConnectionID || !m_SpaceCreate || !m_SpaceSetAbsoluteLevel || !m_ShowSpaces || !m_SpaceAddWindowsAndRemoveFromSpaces ||
     !m_RemoveWindowsFromSpaces) {
    spdlog::warn("SkyLight unavailable: missing symbols.");
    return false;
  }

  m_Connection = m_MainConnectionID();
  m_Space = m_SpaceCreate(m_Connection, SPACE_FLAGS, nullptr);
  if(m_Space == 0) {
    spdlog::warn("SkyLight unavailable: space creation failed.");
    return false;
  }
  m_SpaceSetAbsoluteLevel(m_Connection, m_Space, SPACE_LEVEL);
  m_ShowSpaces(m_Connection, (__bridge CFArrayRef) @[ @(m_Space) ]);
  m_IsAvailable = true;
  spdlog::info("SkyLight space created. (Space={})", m_Space);
  return true;
}

bool SkyLightSpace::IsAvailable() const {
  return m_IsAvailable;
}

void SkyLightSpace::Attach(NSWindow *window) const {
  if(!m_IsAvailable || window.windowNumber <= 0)
    return;
  m_SpaceAddWindowsAndRemoveFromSpaces(m_Connection, m_Space, (__bridge CFArrayRef) @[ @(window.windowNumber) ], MOVE_FLAGS);
}

void SkyLightSpace::Detach(NSWindow *window) const {
  if(!m_IsAvailable || window.windowNumber <= 0)
    return;
  m_RemoveWindowsFromSpaces(m_Connection, (__bridge CFArrayRef) @[ @(window.windowNumber) ], (__bridge CFArrayRef) @[ @(m_Space) ]);
}
