#ifndef PCBU_MAC_ACCESSIBILITYPERMISSION_H
#define PCBU_MAC_ACCESSIBILITYPERMISSION_H

class AccessibilityPermission {
public:
  static bool IsGranted();
  static void Request();
  static void OpenSettings();

private:
  AccessibilityPermission() = default;
};

#endif
