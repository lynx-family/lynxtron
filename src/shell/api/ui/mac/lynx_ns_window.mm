// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/api/ui/mac/lynx_ns_window.h"

#include "base/values.h"
#include "base/strings/sys_string_conversions.h"
#include "shell/ui/gfx/mac/coordinate_conversion.h"

namespace {

// It is not valid to make a zero-sized window. Use this constant instead.
inline constexpr NSRect kWindowSizeDeterminedLater = {{0, 0}, {1, 1}};

const char* InputTypeFromNSEventType(NSEventType type) {
  switch (type) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:
      return "mousePressed";
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseUp:
      return "mouseReleased";
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeMouseMoved:
      return "mouseMoved";
    case NSEventTypeScrollWheel:
      return "mouseWheel";
    default:
      return nullptr;
  }
}

const char* ButtonFromNSEvent(NSEvent* event) {
  switch (event.type) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeLeftMouseDragged:
      return "left";
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeRightMouseDragged:
      return "right";
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeOtherMouseDragged:
      return event.buttonNumber == 2 ? "middle" : "other";
    default:
      return "none";
  }
}

base::Value::List ModifiersFromNSEvent(NSEvent* event) {
  base::Value::List modifiers;
  NSEventModifierFlags flags = event.modifierFlags;
  if (flags & NSEventModifierFlagShift) {
    modifiers.Append("shift");
  }
  if (flags & NSEventModifierFlagControl) {
    modifiers.Append("control");
  }
  if (flags & NSEventModifierFlagOption) {
    modifiers.Append("alt");
  }
  if (flags & NSEventModifierFlagCommand) {
    modifiers.Append("meta");
  }
  if (flags & NSEventModifierFlagCapsLock) {
    modifiers.Append("capsLock");
  }
  return modifiers;
}

void NotifyWindowInputEvent(lynxtron::NativeWindowMac* shell,
                            NSWindow* window,
                            NSEvent* event) {
  if (!shell || !window || event.window != window) {
    return;
  }
  const char* type = InputTypeFromNSEventType(event.type);
  if (!type) {
    return;
  }

  NSView* content_view = window.contentView;
  if (!content_view) {
    return;
  }

  NSPoint location = [content_view convertPoint:event.locationInWindow
                                       fromView:nil];
  NSRect bounds = content_view.bounds;
  const double x = location.x - bounds.origin.x;
  const double y = content_view.isFlipped
                       ? location.y - bounds.origin.y
                       : NSMaxY(bounds) - location.y;
  const bool inside_content = x >= 0 && y >= 0 && x <= bounds.size.width &&
                              y <= bounds.size.height;

  base::Value::Dict details;
  details.Set("provider", "macos-lynxtron");
  details.Set("kind", "pointer");
  details.Set("type", type);
  details.Set("pointerType", "mouse");
  details.Set("deviceKind", "mouse");
  details.Set("coordinateSpace", "viewport");
  details.Set("x", x);
  details.Set("y", y);
  details.Set("contentWidth", bounds.size.width);
  details.Set("contentHeight", bounds.size.height);
  details.Set("insideContent", inside_content);
  details.Set("button", ButtonFromNSEvent(event));
  details.Set("buttonNumber", static_cast<int>(event.buttonNumber));
  details.Set("buttons", static_cast<int>([NSEvent pressedMouseButtons]));
  details.Set("clickCount", static_cast<int>(event.clickCount));
  details.Set("deltaX", event.deltaX);
  details.Set("deltaY", event.deltaY);
  details.Set("nativeTimestamp", event.timestamp);
  details.Set("windowDevicePixelRatio", window.backingScaleFactor);
  details.Set("modifiers", ModifiersFromNSEvent(event));
  shell->NotifyWindowInputEvent(details);
}

}  // namespace

@implementation LynxNSWindow

@synthesize enableLargerThanScreen;
@synthesize disableAutoHideCursor;
@synthesize disableKeyOrMainWindow;
@synthesize customButtonsOnHover;
@synthesize vibrantView;

- (id)initWithShell:(lynxtron::NativeWindowMac*)shell
          styleMask:(NSUInteger)styleMask {
  if (self = [super initWithContentRect:kWindowSizeDeterminedLater
                              styleMask:styleMask
                                backing:NSBackingStoreBuffered
                                  defer:NO]) {
    shell_ = shell;
    suppress_set_frame_origin_ = false;
    has_last_allowed_origin_ = false;
    last_allowed_origin_ = NSMakePoint(0, 0);
    in_move_drag_ = false;
    cached_move_prevent_default_ = false;
  }

  return self;
}

- (lynxtron::NativeWindowMac*)shell {
  return shell_;
}

- (void)cleanup {
  shell_ = nullptr;
}

- (NSRect)originalContentRectForFrameRect:(NSRect)frameRect {
  return [super contentRectForFrameRect:frameRect];
}

// NSWindow overrides.

- (void)sendEvent:(NSEvent*)event {
  if (event.type == NSEventTypeLeftMouseUp) {
    in_move_drag_ = false;
    cached_move_prevent_default_ = false;
    has_last_allowed_origin_ = false;
  }
  NotifyWindowInputEvent(shell_, self, event);
  [super sendEvent:event];
  if (self.disableAutoHideCursor) {
    return;
  }
  if (event.type != NSEventTypeKeyDown) {
    return;
  }
  if (event.modifierFlags & NSEventModifierFlagCommand) {
    return;
  }
  id responder = [self firstResponder];
  if (!responder) {
    return;
  }
  if (![responder conformsToProtocol:@protocol(NSTextInputClient)]) {
    return;
  }
  [NSCursor setHiddenUntilMouseMoves:YES];
}

- (void)setFrameOrigin:(NSPoint)newOrigin {
  if (!shell_ || suppress_set_frame_origin_ || self.inLiveResize) {
    [super setFrameOrigin:newOrigin];
    return;
  }

  // Only attempt to make will-move cancellable for user-initiated drags.
  NSEvent* event = [NSApp currentEvent];
  const bool user_dragging = event && event.window == self &&
                             event.type == NSEventTypeLeftMouseDragged;
  if (!user_dragging) {
    [super setFrameOrigin:newOrigin];
    return;
  }

  if (!in_move_drag_) {
    in_move_drag_ = true;
    cached_move_prevent_default_ = false;
    has_last_allowed_origin_ = true;
    last_allowed_origin_ = [self frame].origin;

    NSSize size = [[self contentView] frame].size;
    NSRect new_bounds =
        NSMakeRect(newOrigin.x, newOrigin.y, size.width, size.height);
    bool prevent_default = false;
    shell_->NotifyWindowWillMove(gfx::ScreenRectFromNSRect(new_bounds),
                                 prevent_default);
    cached_move_prevent_default_ = prevent_default;
  }

  if (cached_move_prevent_default_) {
    suppress_set_frame_origin_ = true;
    [super setFrameOrigin:last_allowed_origin_];
    suppress_set_frame_origin_ = false;
    return;
  }

  NSSize size = [[self contentView] frame].size;
  NSRect new_bounds =
      NSMakeRect(newOrigin.x, newOrigin.y, size.width, size.height);
  has_last_allowed_origin_ = true;
  last_allowed_origin_ = newOrigin;
  [super setFrameOrigin:newOrigin];
}

- (void)swipeWithEvent:(NSEvent*)event {
  if (!shell_) {
    return;
  }
  if (event.deltaY == 1.0) {
    shell_->NotifyWindowSwipe("up");
  } else if (event.deltaX == -1.0) {
    shell_->NotifyWindowSwipe("right");
  } else if (event.deltaY == -1.0) {
    shell_->NotifyWindowSwipe("down");
  } else if (event.deltaX == 1.0) {
    shell_->NotifyWindowSwipe("left");
  }
}

- (void)rotateWithEvent:(NSEvent*)event {
  if (!shell_) {
    return;
  }
  shell_->NotifyWindowRotateGesture(event.rotation);
}

- (NSRect)contentRectForFrameRect:(NSRect)frameRect {
  if (shell_ && shell_->frame()) {
    return [super contentRectForFrameRect:frameRect];
  } else {
    return frameRect;
  }
}

- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen*)screen {
  NSRect result = [super constrainFrameRect:frameRect toScreen:screen];
  if ([self enableLargerThanScreen]) {
    // Frameless windows should keep the exact requested bounds, while framed
    // windows still need to remain in a draggable/resizable position.
    if (shell_ && shell_->frame()) {
      result.size = frameRect.size;
    } else {
      result = frameRect;
    }
  }

  return result;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqual:NSAccessibilityEnabledAttribute]) {
    return [NSNumber numberWithBool:YES];
  }
  if (![attribute isEqualToString:@"AXChildren"]) {
    return [super accessibilityAttributeValue:attribute];
  }

  // We want to remove the window title (also known as
  // NSAccessibilityReparentingCellProxy), which VoiceOver already sees.
  // * when VoiceOver is disabled, this causes Cmd+C to be used for TTS but
  //   still leaves the buttons available in the accessibility tree.
  // * when VoiceOver is enabled, the full accessibility tree is used.
  // Without removing the title and with VO disabled, the TTS would always read
  // the window title instead of using Cmd+C to get the selected text.
  NSPredicate* predicate =
      [NSPredicate predicateWithFormat:@"(self.className != %@)",
                                       @"NSAccessibilityReparentingCellProxy"];

  NSArray* children = [super accessibilityAttributeValue:attribute];
  NSMutableArray* mutableChildren = [children mutableCopy];
  [mutableChildren filterUsingPredicate:predicate];

  return mutableChildren;
}

- (NSString*)accessibilityTitle {
  return base::SysUTF8ToNSString(shell_ ? shell_->GetTitle() : "");
}

- (BOOL)canBecomeMainWindow {
  return !self.disableKeyOrMainWindow;
}

- (BOOL)canBecomeKeyWindow {
  return !self.disableKeyOrMainWindow;
}

- (NSView*)frameView {
  return [[self contentView] superview];
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  // By default "Close Window" is always disabled when window has no title, to
  // support closing a window without title we need to manually do menu item
  // validation. This code path is used by the "roundedCorners" option.
  if ([item action] == @selector(performClose:)) {
    return shell_ && shell_->IsClosable();
  }
  return [super validateUserInterfaceItem:item];
}

- (BOOL)windowShouldClose:(id)sender {
  return YES;
}

- (void)performClose:(id)sender {
  if (self.customButtonsOnHover) {
    [[self delegate] windowShouldClose:self];
    return;
  }

  if (!([self styleMask] & NSWindowStyleMaskTitled)) {
    if ([[self delegate] respondsToSelector:@selector(windowShouldClose:)]) {
      if (![[self delegate] windowShouldClose:self]) {
        return;
      }
    } else if ([self respondsToSelector:@selector(windowShouldClose:)]) {
      if (![self windowShouldClose:self]) {
        return;
      }
    }
    [self close];
    return;
  }

  if (shell_ && shell_->is_modal() && shell_->parent() && shell_->IsVisible()) {
    return;
  }

  [super performClose:sender];
}

- (void)toggleFullScreenMode:(id)sender {
  if (!shell_) {
    return;
  }
  bool is_simple_fs = shell_->IsSimpleFullScreen();
  bool always_simple_fs = shell_->always_simple_fullscreen();

  // If we're in simple fullscreen mode and trying to exit it
  // we need to ensure we exit it properly to prevent a crash
  // with NSWindowStyleMaskTitled mode.
  if (is_simple_fs || always_simple_fs) {
    shell_->SetSimpleFullScreen(!is_simple_fs);
    shell_->set_fullscreen_transition_state(
        lynxtron::NativeWindowMac::FullScreenTransitionState::NONE);
    shell_->HandlePendingFullscreenTransitions();
  } else {
    if (shell_->IsVisible()) {
      // Until 10.13, AppKit would obey a call to -toggleFullScreen: made inside
      // windowDidEnterFullScreen & windowDidExitFullScreen. Starting in 10.13,
      // it behaves as though the transition is still in progress and just emits
      // "not in a fullscreen state" when trying to exit fullscreen in the same
      // runloop that entered it. To handle this, invoke -toggleFullScreen:
      // asynchronously.
      [super performSelector:@selector(toggleFullScreen:)
                  withObject:nil
                  afterDelay:0];
    } else {
      [super toggleFullScreen:sender];
    }

    // Exiting fullscreen causes Cocoa to redraw the NSWindow, which resets
    // the enabled state for NSWindowZoomButton. We need to persist it.
    bool maximizable = shell_->IsMaximizable();
    shell_->SetMaximizable(maximizable);
  }
}

- (void)performMiniaturize:(id)sender {
  if (self.customButtonsOnHover) {
    [self miniaturize:self];
  } else {
    [super performMiniaturize:sender];
  }
}

@end
