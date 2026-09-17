#include "RootGuard.h"

#include <spdlog/spdlog.h>
#include <unistd.h>

RootGuard::RootGuard() {
  m_Elevated = seteuid(0) == 0;
  if(!m_Elevated)
    spdlog::error("Failed to regain root privileges.");
}

RootGuard::~RootGuard() {
  if(!m_Elevated)
    return;
  if(!DropPrivileges()) {
    fmt::print(stderr, "Failed to drop root privileges.\n");
    _exit(1);
  }
}

bool RootGuard::DropPrivileges() {
  return seteuid(getuid()) == 0 && geteuid() == getuid();
}
