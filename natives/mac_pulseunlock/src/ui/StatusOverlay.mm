#include "StatusOverlay.h"

#include <algorithm>

#import <QuartzCore/QuartzCore.h>
#include <spdlog/spdlog.h>

#include "utils/I18n.h"

@interface PCBUOverlayActionTarget : NSObject
@property(nonatomic, copy) void (^handler)(void);
- (void)invoke:(id)sender;
@end

@implementation PCBUOverlayActionTarget
- (void)invoke:(id)sender {
  if(self.handler)
    self.handler();
}
@end

@interface PCBUOverlayButton : NSButton
@end

@implementation PCBUOverlayButton
- (BOOL)acceptsFirstMouse:(NSEvent *)event {
  return YES;
}
@end

StatusOverlay::StatusOverlay(const std::function<void()> &onRetry, const std::function<void()> &onCancel) {
  auto retryHandler = onRetry;
  auto cancelHandler = onCancel;
  m_RetryTarget = [PCBUOverlayActionTarget new];
  m_RetryTarget.handler = ^{
    retryHandler();
  };
  m_CancelTarget = [PCBUOverlayActionTarget new];
  m_CancelTarget.handler = ^{
    cancelHandler();
  };
  if(!m_Space.Init())
    spdlog::warn("Lock screen overlay unavailable.");
  CreatePanel();
}

void StatusOverlay::CreatePanel() {
  m_Panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 200, 40)
                                       styleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel
                                         backing:NSBackingStoreBuffered
                                           defer:NO];
  m_Panel.level = CGShieldingWindowLevel() + 1;
  m_Panel.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary |
                               NSWindowCollectionBehaviorFullScreenAuxiliary | NSWindowCollectionBehaviorIgnoresCycle;
  m_Panel.canBecomeVisibleWithoutLogin = YES;
  m_Panel.opaque = NO;
  m_Panel.backgroundColor = NSColor.clearColor;
  m_Panel.hasShadow = YES;
  m_Panel.floatingPanel = YES;
  m_Panel.becomesKeyOnlyIfNeeded = YES;
  m_Panel.hidesOnDeactivate = NO;
  m_Panel.releasedWhenClosed = NO;
  m_Panel.ignoresMouseEvents = YES;
  m_Panel.alphaValue = 0;
  m_Panel.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];

  m_IconView = [NSImageView new];
  m_IconView.wantsLayer = YES;
  m_IconView.symbolConfiguration = [NSImageSymbolConfiguration configurationWithPointSize:18 weight:NSFontWeightMedium];
  m_IconView.imageScaling = NSImageScaleNone;
  UpdateIcon(OverlayStyle::WAITING, false);

  m_Label = [NSTextField wrappingLabelWithString:@""];
  m_Label.wantsLayer = YES;
  m_Label.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
  m_Label.textColor = NSColor.labelColor;
  m_Label.alignment = NSTextAlignmentCenter;
  m_Label.maximumNumberOfLines = 3;

  auto retryTitle = [NSString stringWithUTF8String:I18n::Get("retry").c_str()];
  m_RetryButton = [PCBUOverlayButton buttonWithTitle:retryTitle ?: @"Retry" target:m_RetryTarget action:@selector(invoke:)];
  m_RetryButton.bezelStyle = NSBezelStylePush;
  m_RetryButton.controlSize = NSControlSizeSmall;
  auto cancelTitle = [NSString stringWithUTF8String:I18n::Get("cancel").c_str()];
  m_CancelButton = [PCBUOverlayButton buttonWithTitle:cancelTitle ?: @"Cancel" target:m_CancelTarget action:@selector(invoke:)];
  m_CancelButton.bezelStyle = NSBezelStylePush;
  m_CancelButton.controlSize = NSControlSizeSmall;

  m_IconView.autoresizingMask = NSViewMaxXMargin | NSViewMinYMargin | NSViewMaxYMargin;
  m_Label.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin | NSViewMaxYMargin;
  m_RetryButton.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin | NSViewMaxYMargin;
  m_CancelButton.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin | NSViewMaxYMargin;

  m_ContentView = [NSView new];
  for(NSView *view in @[ m_IconView, m_Label, m_RetryButton, m_CancelButton ])
    [m_ContentView addSubview:view];

  NSView *backgroundView{};
  if(@available(macOS 26.0, *)) {
    auto glassView = [NSGlassEffectView new];
    glassView.cornerRadius = CORNER_RADIUS;
    m_ContentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    glassView.contentView = m_ContentView;
    backgroundView = glassView;
  } else {
    auto effectView = [NSVisualEffectView new];
    effectView.material = NSVisualEffectMaterialHUDWindow;
    effectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    effectView.state = NSVisualEffectStateActive;
    effectView.wantsLayer = YES;
    effectView.layer.cornerRadius = CORNER_RADIUS;
    effectView.layer.masksToBounds = YES;
    m_ContentView.frame = effectView.bounds;
    m_ContentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [effectView addSubview:m_ContentView];
    backgroundView = effectView;
  }
  backgroundView.wantsLayer = YES;
  m_Panel.contentView = backgroundView;
}

void StatusOverlay::Show(const std::string &text, OverlayStyle style) {
  auto textChanged = text != m_Text;
  auto styleChanged = style != m_Style;
  m_Text = text;
  m_Style = style;
  if(textChanged)
    spdlog::debug("Overlay: {}", text);
  if(m_IsVisible && textChanged)
    AddFadeTransition(m_Label);
  m_Label.stringValue = [NSString stringWithUTF8String:text.c_str()] ?: @"";
  m_RetryButton.hidden = style != OverlayStyle::ERROR && style != OverlayStyle::CANCELED;
  m_CancelButton.hidden = style != OverlayStyle::RUNNING;
  m_Panel.ignoresMouseEvents = m_RetryButton.hidden && m_CancelButton.hidden;
  if(styleChanged)
    UpdateIcon(style, m_IsVisible);

  if(m_IsVisible) {
    UpdateLayout(true);
    if(styleChanged && (style == OverlayStyle::ERROR || style == OverlayStyle::CANCELED)) {
      dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(RESIZE_DURATION * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        if(m_IsVisible && m_Style == style)
          Shake();
      });
    }
  } else {
    m_IsVisible = true;
    UpdateLayout(false);
    [m_Panel orderFrontRegardless];
    m_Space.Attach(m_Panel);
    AnimateAppear();
  }
  if(textChanged && !text.empty()) {
    NSAccessibilityPostNotificationWithUserInfo(
        m_Panel, NSAccessibilityAnnouncementRequestedNotification, @{
          NSAccessibilityAnnouncementKey : m_Label.stringValue,
          NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
        });
  }
}

bool StatusOverlay::HasAnchor() const {
  return m_Anchor.has_value();
}

void StatusOverlay::SetAnchor(const CGRect &fieldFrame) {
  if(m_Anchor.has_value() && CGRectEqualToRect(*m_Anchor, fieldFrame))
    return;
  m_Anchor = fieldFrame;
  spdlog::debug("Overlay anchor: password field at ({}, {}, {}x{}).", fieldFrame.origin.x, fieldFrame.origin.y, fieldFrame.size.width,
                fieldFrame.size.height);
  if(m_IsVisible)
    UpdateLayout(true);
}

void StatusOverlay::Hide() {
  if(!m_IsVisible)
    return;
  m_IsVisible = false;
  m_Text = {};
  m_Style.reset();
  [NSAnimationContext
      runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = FADE_DURATION;
        m_Panel.animator.alphaValue = 0;
      }
      completionHandler:^{
        if(m_IsVisible)
          return;
        [m_Panel orderOut:nil];
        m_Space.Detach(m_Panel);
      }];
}

void StatusOverlay::UpdateIcon(OverlayStyle style, bool isAnimated) {
  NSString *symbolName{};
  NSColor *tintColor{};
  switch(style) {
    case OverlayStyle::WAITING:
    case OverlayStyle::RUNNING:
      symbolName = @"iphone.gen3";
      tintColor = NSColor.labelColor;
      break;
    case OverlayStyle::SUCCESS:
      symbolName = @"checkmark.circle.fill";
      tintColor = NSColor.systemGreenColor;
      break;
    case OverlayStyle::CANCELED:
      symbolName = @"exclamationmark.triangle.fill";
      tintColor = NSColor.systemYellowColor;
      break;
    case OverlayStyle::ERROR:
      symbolName = @"exclamationmark.circle.fill";
      tintColor = NSColor.systemRedColor;
      break;
  }
  [m_IconView removeAllSymbolEffects];
  auto isMotion = isAnimated && !IsReduceMotion();
  if(![symbolName isEqualToString:m_SymbolName]) {
    auto image = [NSImage imageWithSystemSymbolName:symbolName accessibilityDescription:nil];
    if(!isMotion)
      m_IconView.image = image;
    else if(style == OverlayStyle::SUCCESS && HasDrawOnEffect())
      m_IconView.image = image;
    else
      [m_IconView setSymbolImage:image withContentTransition:GetReplaceTransition()];
    m_SymbolName = symbolName;
  }
  m_IconView.contentTintColor = tintColor;
  if(IsReduceMotion())
    return;

  if(style == OverlayStyle::WAITING) {
    if(@available(macOS 15.0, *)) {
      auto behavior = [NSSymbolEffectOptionsRepeatBehavior behaviorPeriodicWithDelay:WAIT_HINT_DELAY];
      [m_IconView addSymbolEffect:[NSSymbolWiggleEffect wiggleLeftEffect] options:[NSSymbolEffectOptions optionsWithRepeatBehavior:behavior]];
    }
  } else if(style == OverlayStyle::RUNNING) {
    if(@available(macOS 15.0, *)) {
      auto behavior = [NSSymbolEffectOptionsRepeatBehavior behaviorContinuous];
      [m_IconView addSymbolEffect:[NSSymbolBreatheEffect breathePulseEffect] options:[NSSymbolEffectOptions optionsWithRepeatBehavior:behavior]];
    } else {
      [m_IconView addSymbolEffect:[NSSymbolPulseEffect effect] options:[NSSymbolEffectOptions optionsWithRepeating]];
    }
  } else if(style == OverlayStyle::SUCCESS && isMotion) {
    if(@available(macOS 26.0, *)) {
      [m_IconView addSymbolEffect:[NSSymbolDrawOnEffect effect]];
      return;
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(BOUNCE_DELAY * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      if(m_Style == OverlayStyle::SUCCESS)
        [m_IconView addSymbolEffect:[NSSymbolBounceEffect effect]];
    });
  }
}

NSSymbolContentTransition *StatusOverlay::GetReplaceTransition() {
  auto transition = [NSSymbolReplaceContentTransition replaceDownUpTransition];
  if(@available(macOS 15.0, *))
    return [NSSymbolReplaceContentTransition magicTransitionWithFallback:transition];
  return transition;
}

bool StatusOverlay::HasDrawOnEffect() {
  if(@available(macOS 26.0, *))
    return true;
  return false;
}

void StatusOverlay::AnimateAppear() {
  [NSAnimationContext
      runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = FADE_DURATION;
        m_Panel.animator.alphaValue = 1;
      }
      completionHandler:nil];
  if(IsReduceMotion())
    return;

  auto layer = m_Panel.contentView.layer;
  auto center = CGPointMake(CGRectGetMidX(layer.bounds), CGRectGetMidY(layer.bounds));
  auto transform = CATransform3DConcat(
      CATransform3DConcat(CATransform3DMakeTranslation(-center.x, -center.y, 0), CATransform3DMakeScale(APPEAR_SCALE, APPEAR_SCALE, 1)),
      CATransform3DMakeTranslation(center.x, center.y, 0));
  auto animation = [CABasicAnimation animationWithKeyPath:@"transform"];
  animation.fromValue = [NSValue valueWithCATransform3D:transform];
  animation.toValue = [NSValue valueWithCATransform3D:CATransform3DIdentity];
  animation.duration = APPEAR_DURATION;
  animation.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
  [layer addAnimation:animation forKey:@"appear"];
}

void StatusOverlay::Shake() {
  if(IsReduceMotion())
    return;
  auto origin = m_Panel.frame.origin;
  auto path = CGPathCreateMutable();
  CGPathMoveToPoint(path, nullptr, origin.x, origin.y);
  for(auto offset : {-1.0, 1.0, -0.75, 0.75, -0.4, 0.4, 0.0})
    CGPathAddLineToPoint(path, nullptr, origin.x + offset * SHAKE_DISTANCE, origin.y);
  auto animation = [CAKeyframeAnimation animation];
  animation.path = path;
  animation.duration = SHAKE_DURATION;
  CGPathRelease(path);
  m_Panel.animations = @{@"frameOrigin" : animation};
  [m_Panel.animator setFrameOrigin:origin];
}

bool StatusOverlay::IsReduceMotion() {
  return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion;
}

void StatusOverlay::AddFadeTransition(NSView *view) {
  auto transition = [CATransition animation];
  transition.type = kCATransitionFade;
  transition.duration = FADE_DURATION;
  [view.layer addAnimation:transition forKey:@"contentFade"];
}

NSSize StatusOverlay::LayoutContent() {
  NSButton *button = !m_RetryButton.hidden ? m_RetryButton : (!m_CancelButton.hidden ? m_CancelButton : nil);
  auto iconSize = NSMakeSize(ICON_SIZE, ICON_SIZE);
  auto buttonSize = button ? button.intrinsicContentSize : NSZeroSize;
  auto leftWidth = H_INSET + iconSize.width + SPACING;
  auto rightWidth = button ? SPACING + buttonSize.width + H_INSET : H_INSET;
  auto sideWidth = std::ceil(std::max(leftWidth, rightWidth));

  auto maxLabelWidth = MAX_WIDTH - 2 * sideWidth;
  auto labelSize = [m_Label.cell cellSizeForBounds:NSMakeRect(0, 0, maxLabelWidth, CGFLOAT_MAX)];
  labelSize.width = std::ceil(std::min(labelSize.width, maxLabelWidth));
  labelSize.height = std::ceil(labelSize.height);

  NSSize size{};
  size.width = std::min(MAX_WIDTH, std::max(2 * sideWidth + labelSize.width, MIN_WIDTH));
  size.height = std::ceil(std::max({iconSize.height, labelSize.height, buttonSize.height})) + 2 * V_INSET;

  auto bounds = m_ContentView.bounds.size;
  m_IconView.frame = NSMakeRect(H_INSET, (bounds.height - iconSize.height) / 2, iconSize.width, iconSize.height);
  m_Label.frame = NSMakeRect((bounds.width - labelSize.width) / 2, (bounds.height - labelSize.height) / 2, labelSize.width, labelSize.height);
  for(NSButton *candidate in @[ m_RetryButton, m_CancelButton ]) {
    auto candidateSize = candidate.intrinsicContentSize;
    candidate.frame = NSMakeRect(bounds.width - H_INSET - candidateSize.width, (bounds.height - candidateSize.height) / 2, candidateSize.width,
                                 candidateSize.height);
  }
  return size;
}

void StatusOverlay::UpdateLayout(bool isAnimated) {
  auto size = LayoutContent();

  auto screens = NSScreen.screens;
  if(screens.count == 0)
    return;
  NSScreen *screen{};
  NSRect fieldRect{};
  if(m_Anchor.has_value()) {
    fieldRect = *m_Anchor;
    fieldRect.origin.y = NSMaxY(screens[0].frame) - NSMaxY(*m_Anchor);
    for(NSScreen *candidate in screens) {
      if(NSPointInRect(NSMakePoint(NSMidX(fieldRect), NSMidY(fieldRect)), candidate.frame))
        screen = candidate;
    }
  }

  NSRect frame{};
  frame.size = size;
  if(screen) {
    frame.origin.x = NSMidX(screen.frame) - size.width / 2;
    frame.origin.y = NSMinY(fieldRect) - FIELD_SPACING - size.height;
  } else {
    screen = NSScreen.mainScreen ?: screens[0];
    frame.origin.x = NSMidX(screen.frame) - size.width / 2;
    frame.origin.y = NSMinY(screen.frame) + BOTTOM_MARGIN;
  }
  if(NSEqualRects(frame, m_Panel.frame))
    return;
  if(!isAnimated || IsReduceMotion()) {
    [m_Panel setFrame:frame display:YES];
    return;
  }
  [NSAnimationContext
      runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = RESIZE_DURATION;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        [m_Panel.animator setFrame:frame display:YES];
      }
      completionHandler:nil];
}
