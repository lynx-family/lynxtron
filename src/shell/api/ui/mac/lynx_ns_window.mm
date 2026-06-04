// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/api/ui/mac/lynx_ns_window.h"

#include "base/strings/sys_string_conversions.h"
#include "shell/ui/gfx/mac/coordinate_conversion.h"

namespace {

// It is not valid to make a zero-sized window. Use this constant instead.
inline constexpr NSRect kWindowSizeDeterminedLater = {{0, 0}, {1, 1}};

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
