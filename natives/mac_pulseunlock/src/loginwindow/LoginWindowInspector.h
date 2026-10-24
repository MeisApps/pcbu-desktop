#ifndef PCBU_MAC_LOGINWINDOWINSPECTOR_H
#define PCBU_MAC_LOGINWINDOWINSPECTOR_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#import <ApplicationServices/ApplicationServices.h>

struct LocalUser {
  std::string userName{};
  std::string fullName{};
};

struct LoginWindowScan {
  std::optional<CGRect> passwordFieldFrame{};
  std::shared_ptr<const __AXUIElement> passwordField{};
  std::vector<std::string> texts{};
};

class LoginWindowInspector {
public:
  static std::optional<CGRect> GetFocusedPasswordFieldFrame();
  static std::optional<CGRect> GetPasswordFieldFrame();
  static bool IsOtherElementFocused();
  static std::string GetSelectedUser();
  static bool SubmitPassword(const std::string &password);
  static bool FocusPasswordField(const std::function<bool()> &shouldContinue);

private:
  LoginWindowInspector() = default;

  static AXUIElementRef CopyFocusedElement();
  static std::shared_ptr<const __AXUIElement> FindPasswordField();
  static CFArrayRef CopyAttributes(AXUIElementRef element, CFArrayRef attributes);
  static std::string ToString(CFTypeRef value);
  static std::optional<CGRect> ToFrame(CFTypeRef position, CFTypeRef size);
  static LoginWindowScan ScanLoginWindow();
  static std::vector<pid_t> GetLoginWindowPids();
  static void CollectElements(AXUIElementRef element, int depth, size_t &visited, LoginWindowScan &scan);
  static std::vector<LocalUser> GetLocalUsers();
  static void ClickAt(CGPoint point);

  static constexpr float MESSAGING_TIMEOUT = 0.25f;
  static constexpr int FOCUS_MAX_ATTEMPTS = 10;
  static constexpr int FOCUS_CLICK_ATTEMPT = 5;
  static constexpr useconds_t FOCUS_RETRY_DELAY_US = 100000;
  static constexpr useconds_t CLICK_DELAY_US = 10000;
  static constexpr int MAX_TREE_DEPTH = 12;
  static constexpr size_t MAX_TREE_ELEMENTS = 512;
};

#endif
