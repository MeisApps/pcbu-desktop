#ifndef PCBU_MAC_STATUSOVERLAY_H
#define PCBU_MAC_STATUSOVERLAY_H

#include <functional>
#include <optional>
#include <string>

#import <AppKit/AppKit.h>

#include "SkyLightSpace.h"

@class PCBUOverlayActionTarget;

enum class OverlayStyle { WAITING, RUNNING, SUCCESS, CANCELED, ERROR };

class StatusOverlay {
public:
  StatusOverlay(const std::function<void()> &onRetry, const std::function<void()> &onCancel);

  void Show(const std::string &text, OverlayStyle style);
  [[nodiscard]] bool HasAnchor() const;
  void SetAnchor(const CGRect &fieldFrame);
  void Hide();

private:
  void CreatePanel();
  NSSize LayoutContent();
  void UpdateLayout(bool isAnimated);
  void UpdateIcon(OverlayStyle style, bool isAnimated);
  void AnimateAppear();
  void Shake();

  static void AddFadeTransition(NSView *view);
  static bool IsReduceMotion();
  static bool HasDrawOnEffect();
  static NSSymbolContentTransition *GetReplaceTransition();

  SkyLightSpace m_Space{};
  NSPanel *m_Panel{};
  NSImageView *m_IconView{};
  NSString *m_SymbolName{};
  NSTextField *m_Label{};
  NSButton *m_RetryButton{};
  NSButton *m_CancelButton{};
  NSView *m_ContentView{};
  PCBUOverlayActionTarget *m_RetryTarget{};
  PCBUOverlayActionTarget *m_CancelTarget{};
  std::optional<CGRect> m_Anchor{};
  std::string m_Text{};
  std::optional<OverlayStyle> m_Style{};
  bool m_IsVisible{};

  static constexpr CGFloat MIN_WIDTH = 320;
  static constexpr CGFloat MAX_WIDTH = 480;
  static constexpr CGFloat H_INSET = 16;
  static constexpr CGFloat V_INSET = 10;
  static constexpr CGFloat SPACING = 10;
  static constexpr CGFloat ICON_SIZE = 24;
  static constexpr CGFloat FIELD_SPACING = 48;
  static constexpr CGFloat BOTTOM_MARGIN = 24;
  static constexpr CGFloat CORNER_RADIUS = 20;
  static constexpr NSTimeInterval FADE_DURATION = 0.2;
  static constexpr NSTimeInterval RESIZE_DURATION = 0.25;
  static constexpr NSTimeInterval APPEAR_DURATION = 0.3;
  static constexpr NSTimeInterval SHAKE_DURATION = 0.4;
  static constexpr NSTimeInterval BOUNCE_DELAY = 0.2;
  static constexpr double WAIT_HINT_DELAY = 3.0;
  static constexpr CGFloat APPEAR_SCALE = 0.92;
  static constexpr CGFloat SHAKE_DISTANCE = 8;
};

#endif
