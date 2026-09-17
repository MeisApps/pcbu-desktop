#ifndef PCBU_DESKTOP_ROOTGUARD_H
#define PCBU_DESKTOP_ROOTGUARD_H

class RootGuard {
public:
  RootGuard();
  ~RootGuard();
  RootGuard(const RootGuard &) = delete;
  RootGuard &operator=(const RootGuard &) = delete;

  static bool DropPrivileges();

private:
  bool m_Elevated{};
};

#endif // PCBU_DESKTOP_ROOTGUARD_H
