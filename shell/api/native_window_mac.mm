// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/api/native_window_mac.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "shell/api/ui/mac/lynx_ns_window.h"
#include "shell/api/ui/mac/lynx_ns_window_delegate.h"
#include "shell/app/window_list.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/logging.h"
#include "shell/common/node_util.h"
#include "shell/common/options_switches.h"

namespace lynxtron {

NativeWindowMac::NativeWindowMac(const gin_helper::Dictionary& options,
                                 NativeWindow* parent)
    : NativeWindow(options, parent) {
  // auto x = options.ValueOrDefault(options::kX, 0);
  // auto y = options.ValueOrDefault(options::kY, 0);
  auto width = options.ValueOrDefault(options::kWidth, 800);
  auto height = options.ValueOrDefault(options::kHeight, 600);

  NSRect main_screen_rect = [[[NSScreen screens] firstObject] frame];
  gfx::Rect bounds(round((NSWidth(main_screen_rect) - width) / 2),
                   round((NSHeight(main_screen_rect) - height) / 2), width,
                   height);

  const bool resizable = options.ValueOrDefault(options::kResizable, true);
  options.Get(options::kSimpleFullscreen, &always_simple_fullscreen_);

  const bool minimizable = options.ValueOrDefault(options::kMinimizable, true);

  const bool maximizable = options.ValueOrDefault(options::kMaximizable, true);

  const bool closable = options.ValueOrDefault(options::kClosable, true);

  const std::string tabbingIdentifier =
      options.ValueOrDefault(options::kTabbingIdentifier, std::string{});

  const std::string windowType =
      options.ValueOrDefault(options::kType, std::string{});

  NSUInteger styleMask = NSWindowStyleMaskTitled;

  if (minimizable) {
    styleMask |= NSWindowStyleMaskMiniaturizable;
  }
  if (closable) {
    styleMask |= NSWindowStyleMaskClosable;
  }
  if (resizable) {
    styleMask |= NSWindowStyleMaskResizable;
  }

  const bool rounded_corner =
      options.ValueOrDefault(options::kRoundedCorners, true);
  if (!rounded_corner && !has_frame()) {
    SetBorderless(!rounded_corner);
  }

  // TODO: remove NSWindowStyleMaskTexturedBackground.
  // https://github.com/electron/electron/issues/43125
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  if (windowType == "textured" && (transparent() || !has_frame())) {
    util::EmitDeprecationWarning(
        "The 'textured' window type is deprecated and will be removed");
    styleMask |= NSWindowStyleMaskTexturedBackground;
  }
#pragma clang diagnostic pop

  // window_.reset([[LynxNSWindow alloc] initWithShell:this
  // styleMask:styleMask]);
  window_ = [[LynxNSWindow alloc] initWithShell:this styleMask:styleMask];
  window_.releasedWhenClosed = NO;
  // NSLog(
  //     @"[NativeWindowMac] window_ address: %p, this address:
  //     %p", window_, this);
  // [window_ setFrame:NSMakeRect(bounds.x(), bounds.y(), width,
  // height)
  //           display:true];
  [window_ setFrame:NSMakeRect(bounds.x(), bounds.y(), width, height)
            display:true];

  window_delegate_ = [[LynxNSWindowDelegate alloc] initWithShell:this];
  [window_ setDelegate:window_delegate_];

  // Only use native parent window for non-modal windows.
  if (parent && !is_modal()) {
    SetParentWindow(parent);
  }

  if (transparent()) {
    // Setting the background color to clear will also hide the shadow.
    [window_ setBackgroundColor:[NSColor clearColor]];
  }

  if (transparent() || !has_frame()) {
    // Don't show title bar.
    [window_ setTitlebarAppearsTransparent:YES];
    [window_ setTitleVisibility:NSWindowTitleHidden];
    // Remove non-transparent corners, see
    // https://github.com/electron/electron/issues/517.
    [window_ setOpaque:NO];
    SetStyleMask(true, NSWindowStyleMaskFullSizeContentView);
    // Show window buttons if titleBarStyle is not "normal".
    if (title_bar_style_ == TitleBarStyle::kNormal) {
      InternalSetWindowButtonVisibility(false);
    }
  }

  // Set maximizable state last to ensure zoom button does not get reset
  // by calls to other APIs.
  SetMaximizable(maximizable);

  UpdateWindowOriginalFrame();
  original_level_ = [window_ level];
}

NativeWindowMac::~NativeWindowMac() {
  // [window_ setShell:nil];
  // [window_ release];
  // window_ = nullptr;
  // NSLog(@"[NativeWindowMac] ~NativeWindowMac, window_ address:
  // "
  //       @"%p, this "
  //       @"address: %p",
  //       window_, this);
}

void NativeWindowMac::Close() {
  if (!IsClosable()) {
    WindowList::WindowCloseCancelled(this);
    return;
  }

  // If a sheet is attached to the window when we call
  // [window_ performClose:nil], the window won't close properly
  // even after the user has ended the sheet.
  // Ensure it's closed before calling [window_ performClose:nil].
  SetEnabled(true);

  // window_ could be nil after performClose.
  bool should_notify = is_modal() && parent() && IsVisible();

  [window_ performClose:nil];

  // Closing a sheet doesn't trigger windowShouldClose,
  // so we need to manually call it ourselves here.
  if (should_notify) {
    NotifyWindowCloseButtonClicked();
  }
}

void NativeWindowMac::CloseImmediately() {
  [window_ close];
}

void NativeWindowMac::Focus(bool focus) {
  if (!IsVisible()) {
    return;
  }

  if (focus) {
    [[NSApplication sharedApplication] activateIgnoringOtherApps:NO];
    [window_ makeKeyAndOrderFront:nil];
  } else {
    [window_ orderOut:nil];
    [window_ orderBack:nil];
  }
}

bool NativeWindowMac::IsFocused() {
  return [window_ isKeyWindow];
}

void NativeWindowMac::Show() {
  if (is_modal() && parent()) {
    NSWindow* window = parent()->GetNativeWindow().GetNativeNSWindow();
    if ([window_ sheetParent] == nil) {
      [window beginSheet:window_
          completionHandler:^(NSModalResponse){
          }];
    }
    return;
  }
  // Reattach the window to the parent to actually show it.
  if (parent()) {
    InternalSetParentWindow(parent(), true);
  }
  // This method is supposed to put focus on window, however if the app does not
  // have focus then "makeKeyAndOrderFront" will only show the window.
  [NSApp activateIgnoringOtherApps:YES];
  [window_ makeKeyAndOrderFront:nil];
}

void NativeWindowMac::ShowInactive() {
  // Reattach the window to the parent to actually show it.
  if (parent()) {
    InternalSetParentWindow(parent(), true);
  }

  [window_ orderFrontRegardless];
}

void NativeWindowMac::Hide() {
  // If a sheet is attached to the window when we call [window_
  // orderOut:nil], the sheet won't be able to show again on the same window.
  // Ensure it's closed before calling [window_ orderOut:nil].
  SetEnabled(true);

  if (is_modal() && parent()) {
    [window_ orderOut:nil];
    [parent()->GetNativeWindow().GetNativeNSWindow() endSheet:window_];
    return;
  }

  // Hide all children of the current window before hiding the window.
  // components/remote_cocoa/app_shim/native_widget_ns_window_bridge.mm
  // expects this when window visibility changes.
  if ([window_ childWindows]) {
    for (NSWindow* child in [window_ childWindows]) {
      [child orderOut:nil];
    }
  }

  // Detach the window from the parent before.
  if (parent()) {
    InternalSetParentWindow(parent(), false);
  }

  [window_ orderOut:nil];
}

bool NativeWindowMac::IsVisible() {
  bool occluded = [window_ occlusionState] == NSWindowOcclusionStateVisible;

  // For a window to be visible, it must be visible to the user in the
  // foreground of the app, which means that it should not be minimized or
  // occluded
  return [window_ isVisible] && !occluded && !IsMinimized();
}

bool NativeWindowMac::IsEnabled() {
  return [window_ attachedSheet] == nil;
}

void NativeWindowMac::SetEnabled(bool enable) {
  if (!enable) {
    [window_ beginSheet:window_
        completionHandler:^(NSModalResponse returnCode) {
          NSLog(@"modal enabled");
          return;
        }];
  } else if ([window_ attachedSheet]) {
    [window_ endSheet:[window_ attachedSheet]];
  }
}

void NativeWindowMac::Maximize() {
  const bool is_visible = [window_ isVisible];

  if (IsMaximized()) {
    if (!is_visible) {
      ShowInactive();
    }
    return;
  }

  // Take note of the current window size
  if (IsNormal()) {
    UpdateWindowOriginalFrame();
  }
  [window_ zoom:nil];

  if (!is_visible) {
    ShowInactive();
    NotifyWindowMaximize();
  }
}

void NativeWindowMac::Unmaximize() {
  // Bail if the last user set bounds were the same size as the window
  // screen (e.g. the user set the window to maximized via setBounds)
  //
  // Per docs during zoom:
  // > If there’s no saved user state because there has been no previous
  // > zoom,the size and location of the window don’t change.
  //
  // However, in classic Apple fashion, this is not the case in practice,
  // and the frame inexplicably becomes very tiny. We should prevent
  // zoom from being called if the window is being unmaximized and its
  // unmaximized window bounds are themselves functionally maximized.
  if (!IsMaximized() || user_set_bounds_maximized_) {
    return;
  }

  [window_ zoom:nil];
}

bool NativeWindowMac::IsMaximized() const {
  if (HasStyleMask(NSWindowStyleMaskResizable) != 0) {
    return [window_ isZoomed];
  }

  NSRect rectScreen = GetAspectRatio() > 0.0
                          ? default_frame_for_zoom()
                          : [[NSScreen mainScreen] visibleFrame];

  return NSEqualRects([window_ frame], rectScreen);
}

void NativeWindowMac::Minimize() {
  if (IsMinimized()) {
    return;
  }

  // Take note of the current window size
  if (IsNormal()) {
    UpdateWindowOriginalFrame();
  }
  [window_ miniaturize:nil];
}

void NativeWindowMac::Restore() {
  [window_ deminiaturize:nil];
}

bool NativeWindowMac::IsMinimized() const {
  return [window_ isMiniaturized];
}

void NativeWindowMac::SetFullScreen(bool fullscreen) {
  // [NSWindow -toggleFullScreen] is an asynchronous operation, which means
  // that it's possible to call it while a fullscreen transition is currently
  // in process. This can create weird behavior (incl. phantom windows),
  // so we want to schedule a transition for when the current one has completed.
  if (fullscreen_transition_state() != FullScreenTransitionState::NONE) {
    if (!pending_transitions_.empty()) {
      bool last_pending = pending_transitions_.back();
      // Only push new transitions if they're different than the last transition
      // in the queue.
      if (last_pending != fullscreen) {
        pending_transitions_.push(fullscreen);
      }
    } else {
      pending_transitions_.push(fullscreen);
    }
    return;
  }

  if (fullscreen == IsFullscreen()) {
    return;
  }

  // Take note of the current window size
  if (IsNormal()) {
    UpdateWindowOriginalFrame();
  }

  // This needs to be set here because it can be the case that
  // SetFullScreen is called by a user before windowWillEnterFullScreen
  // or windowWillExitFullScreen are invoked, and so a potential transition
  // could be dropped.
  fullscreen_transition_state_ = fullscreen
                                     ? FullScreenTransitionState::ENTERING
                                     : FullScreenTransitionState::EXITING;

  [window_ toggleFullScreenMode:nil];
}

bool NativeWindowMac::IsFullscreen() const {
  return HasStyleMask(NSWindowStyleMaskFullScreen);
}

void NativeWindowMac::SetBounds(const gfx::Rect& bounds, bool animate) {
  // Do nothing if in fullscreen mode.
  if (IsFullscreen()) {
    return;
  }

  // Check size constraints since setFrame does not check it.
  gfx::Size size = bounds.size();
  size.SetToMax(GetMinimumSize());
  gfx::Size max_size = GetMaximumSize();
  if (!max_size.IsEmpty()) {
    size.SetToMin(max_size);
  }

  NSRect cocoa_bounds = NSMakeRect(bounds.x(), 0, size.width(), size.height());
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] firstObject];
  cocoa_bounds.origin.y = NSHeight([screen frame]) - size.height() - bounds.y();

  [window_ setFrame:cocoa_bounds display:YES animate:animate];
  user_set_bounds_maximized_ = IsMaximized() ? true : false;
  UpdateWindowOriginalFrame();
}

gfx::Rect NativeWindowMac::GetBounds() const {
  NSRect frame = [window_ frame];
  gfx::Rect bounds(frame.origin.x, 0, NSWidth(frame), NSHeight(frame));
  NSScreen* screen = [[NSScreen screens] firstObject];
  bounds.set_y(NSHeight([screen frame]) - NSMaxY(frame));
  return bounds;
}

gfx::Rect NativeWindowMac::GetNormalBounds() const {
  if (IsNormal()) {
    return GetBounds();
  }
  NSRect frame = original_frame_;
  gfx::Rect bounds(frame.origin.x, 0, NSWidth(frame), NSHeight(frame));
  NSScreen* screen = [[NSScreen screens] firstObject];
  bounds.set_y(NSHeight([screen frame]) - NSMaxY(frame));
  return bounds;
}

void NativeWindowMac::SetContentSizeConstraints(
    const SizeConstraints& size_constraints) {
  auto convertSize = [this](const gfx::Size& size) {
    // Our frameless window still has titlebar attached, so setting contentSize
    // will result in actual content size being larger.
    if (!has_frame()) {
      NSRect frame = NSMakeRect(0, 0, size.width(), size.height());
      NSRect content = [window_ originalContentRectForFrameRect:frame];
      return content.size;
    } else {
      return NSMakeSize(size.width(), size.height());
    }
  };

  NSView* content = [window_ contentView];
  if (size_constraints.HasMinimumSize()) {
    NSSize min_size = convertSize(size_constraints.GetMinimumSize());
    [window_ setContentMinSize:[content convertSize:min_size toView:nil]];
  }
  if (size_constraints.HasMaximumSize()) {
    NSSize max_size = convertSize(size_constraints.GetMaximumSize());
    [window_ setContentMaxSize:[content convertSize:max_size toView:nil]];
  }
  NativeWindow::SetContentSizeConstraints(size_constraints);
}

void NativeWindowMac::SetResizable(bool resizable) {
  // ScopedDisableResize disable_resize;
  SetStyleMask(resizable, NSWindowStyleMaskResizable);
  // SetCanResize(resizable);
}

void NativeWindowMac::MoveTop() {
  [window_ orderWindow:NSWindowAbove relativeTo:0];
}

bool NativeWindowMac::IsResizable() const {
  bool in_fs_transition =
      fullscreen_transition_state() != FullScreenTransitionState::NONE;
  bool has_rs_mask = HasStyleMask(NSWindowStyleMaskResizable);
  return has_rs_mask && !IsFullscreen() && !in_fs_transition;
}

void NativeWindowMac::SetAspectRatio(double aspect_ratio,
                                     const gfx::Size& extra_size) {
  NativeWindow::SetAspectRatio(aspect_ratio, extra_size);

  // Reset the behaviour to default if aspect_ratio is set to 0 or less.
  if (aspect_ratio > 0.0) {
    NSSize aspect_ratio_size = NSMakeSize(aspect_ratio, 1.0);
    if (has_frame()) {
      [window_ setContentAspectRatio:aspect_ratio_size];
    } else {
      [window_ setAspectRatio:aspect_ratio_size];
    }
  } else {
    [window_ setResizeIncrements:NSMakeSize(1.0, 1.0)];
  }
}

void NativeWindowMac::SetMovable(bool movable) {
  [window_ setMovable:movable];
}

bool NativeWindowMac::IsMovable() const {
  return [window_ isMovable];
}

void NativeWindowMac::SetMinimizable(bool minimizable) {
  SetStyleMask(minimizable, NSWindowStyleMaskMiniaturizable);
}

bool NativeWindowMac::IsMinimizable() const {
  return HasStyleMask(NSWindowStyleMaskMiniaturizable);
}

void NativeWindowMac::SetMaximizable(bool maximizable) {
  maximizable_ = maximizable;
  [[window_ standardWindowButton:NSWindowZoomButton] setEnabled:maximizable];
}

bool NativeWindowMac::IsMaximizable() const {
  return [[window_ standardWindowButton:NSWindowZoomButton] isEnabled];
}

void NativeWindowMac::SetFullScreenable(bool fullscreenable) {
  SetCollectionBehavior(fullscreenable,
                        NSWindowCollectionBehaviorFullScreenPrimary);
  // On EL Capitan this flag is required to hide fullscreen button.
  SetCollectionBehavior(!fullscreenable,
                        NSWindowCollectionBehaviorFullScreenAuxiliary);
}

bool NativeWindowMac::IsFullScreenable() const {
  NSUInteger collectionBehavior = [window_ collectionBehavior];
  return collectionBehavior & NSWindowCollectionBehaviorFullScreenPrimary;
}

void NativeWindowMac::SetClosable(bool closable) {
  SetStyleMask(closable, NSWindowStyleMaskClosable);
}

bool NativeWindowMac::IsClosable() const {
  return HasStyleMask(NSWindowStyleMaskClosable);
}

void NativeWindowMac::Center() {
  [window_ center];
}

void NativeWindowMac::SetTitle(const std::string& title) {
  [window_ setTitle:base::SysUTF8ToNSString(title)];
  // if (buttons_proxy_)
  //   [buttons_proxy_ redraw];
}

std::string NativeWindowMac::GetTitle() const {
  return base::SysNSStringToUTF8([window_ title]);
}

void NativeWindowMac::FlashFrame(bool flash) {
  if (flash) {
    attention_request_id_ = [NSApp requestUserAttention:NSInformationalRequest];
  } else {
    [NSApp cancelUserAttentionRequest:attention_request_id_];
    attention_request_id_ = 0;
  }
}

void NativeWindowMac::SetSkipTaskbar(bool skip) {}

void NativeWindowMac::SetExcludedFromShownWindowsMenu(bool excluded) {
  [window_ setExcludedFromWindowsMenu:excluded];
}

bool NativeWindowMac::IsExcludedFromShownWindowsMenu() {
  return [window_ isExcludedFromWindowsMenu];
}

void NativeWindowMac::SetSimpleFullScreen(bool simple_fullscreen) {
  if (simple_fullscreen && !is_simple_fullscreen_) {
    is_simple_fullscreen_ = true;

    // Take note of the current window size and level
    if (IsNormal()) {
      UpdateWindowOriginalFrame();
      original_level_ = [window_ level];
    }

    simple_fullscreen_options_ = [NSApp currentSystemPresentationOptions];
    simple_fullscreen_mask_ = [window_ styleMask];

    // We can simulate the pre-Lion fullscreen by auto-hiding the dock and menu
    // bar
    NSApplicationPresentationOptions options =
        NSApplicationPresentationAutoHideDock |
        NSApplicationPresentationAutoHideMenuBar;
    [NSApp setPresentationOptions:options];

    was_maximizable_ = IsMaximizable();
    was_movable_ = IsMovable();

    NSRect fullscreenFrame = [window_.screen frame];

    // If our app has dock hidden, set the window level higher so another app's
    // menu bar doesn't appear on top of our fullscreen app.
    if ([[NSRunningApplication currentApplication] activationPolicy] !=
        NSApplicationActivationPolicyRegular) {
      window_.level = NSPopUpMenuWindowLevel;
    }

    // Always hide the titlebar in simple fullscreen mode.
    //
    // Note that we must remove the NSWindowStyleMaskTitled style instead of
    // using the [window_ setTitleVisibility:], as the latter
    // would leave the window with rounded corners.
    SetStyleMask(false, NSWindowStyleMaskTitled);

    // if (!window_button_visibility_.has_value()) {
    //   // Lets keep previous behaviour - hide window controls in titled
    //   // fullscreen mode when not specified otherwise.
    //   InternalSetWindowButtonVisibility(false);
    // }

    [window_ setFrame:fullscreenFrame display:YES animate:YES];

    // Fullscreen windows can't be resized, minimized, maximized, or moved
    SetMinimizable(false);
    SetResizable(false);
    SetMaximizable(false);
    SetMovable(false);
  } else if (!simple_fullscreen && is_simple_fullscreen_) {
    is_simple_fullscreen_ = false;

    [window_ setFrame:original_frame_ display:YES animate:YES];
    window_.level = original_level_;

    [NSApp setPresentationOptions:simple_fullscreen_options_];

    // Restore original style mask
    // ScopedDisableResize disable_resize;
    [window_ setStyleMask:simple_fullscreen_mask_];

    // Restore window manipulation abilities
    SetMaximizable(was_maximizable_);
    SetMovable(was_movable_);

    // Restore default window controls visibility state.
    // if (!window_button_visibility_.has_value()) {
    //   bool visibility;
    //   if (has_frame())
    //     visibility = true;
    //   else
    //     visibility = title_bar_style_ != TitleBarStyle::kNormal;
    //   InternalSetWindowButtonVisibility(visibility);
    // }

    // if (buttons_proxy_)
    //   [buttons_proxy_ redraw];
  }
}

bool NativeWindowMac::IsSimpleFullScreen() {
  return is_simple_fullscreen_;
}

void NativeWindowMac::SetKiosk(bool kiosk) {
  if (kiosk && !is_kiosk_) {
    kiosk_options_ = [NSApp currentSystemPresentationOptions];
    NSApplicationPresentationOptions options =
        NSApplicationPresentationHideDock |
        NSApplicationPresentationHideMenuBar |
        NSApplicationPresentationDisableAppleMenu |
        NSApplicationPresentationDisableProcessSwitching |
        NSApplicationPresentationDisableForceQuit |
        NSApplicationPresentationDisableSessionTermination |
        NSApplicationPresentationDisableHideApplication;
    [NSApp setPresentationOptions:options];
    is_kiosk_ = true;
    SetFullScreen(true);
  } else if (!kiosk && is_kiosk_) {
    [NSApp setPresentationOptions:kiosk_options_];
    is_kiosk_ = false;
    SetFullScreen(false);
  }
}

bool NativeWindowMac::IsKiosk() const {
  return is_kiosk_;
}

void NativeWindowMac::SetHasShadow(bool has_shadow) {
  [window_ setHasShadow:has_shadow];
}

bool NativeWindowMac::HasShadow() {
  return [window_ hasShadow];
}

void NativeWindowMac::SetOpacity(const double opacity) {
  const double boundedOpacity = std::clamp(opacity, 0.0, 1.0);
  [window_ setAlphaValue:boundedOpacity];
}

double NativeWindowMac::GetOpacity() {
  return [window_ alphaValue];
}

// void NativeWindowMac::SetIgnoreMouseEvents(bool ignore, bool forward) {
//   [window_ setIgnoresMouseEvents:ignore];

//   if (!ignore) {
//     SetForwardMouseMessages(NO);
//   } else {
//     SetForwardMouseMessages(forward);
//   }
// }

void NativeWindowMac::SetContentProtection(bool enable) {
  [window_
      setSharingType:enable ? NSWindowSharingNone : NSWindowSharingReadOnly];
}

void NativeWindowMac::SetFocusable(bool focusable) {
  // No known way to unfocus the window if it had the focus. Here we do not
  // want to call Focus(false) because it moves the window to the back, i.e.
  // at the bottom in term of z-order.
  [window_ setDisableKeyOrMainWindow:!focusable];
}

bool NativeWindowMac::IsFocusable() const {
  return ![window_ disableKeyOrMainWindow];
}

void NativeWindowMac::SetParentWindow(NativeWindow* parent) {
  InternalSetParentWindow(parent, IsVisible());
}

void NativeWindowMac::SetVisibleOnAllWorkspaces(bool visible,
                                                bool visibleOnFullScreen,
                                                bool skipTransformProcessType) {
}

bool NativeWindowMac::IsVisibleOnAllWorkspaces() {
  NSUInteger collectionBehavior = [window_ collectionBehavior];
  return collectionBehavior & NSWindowCollectionBehaviorCanJoinAllSpaces;
}

NativeWindowHandle NativeWindowMac::GetNativeWindowHandle() const {
  return [window_ contentView];
}

gfx::Rect NativeWindowMac::ContentBoundsToWindowBounds(
    const gfx::Rect& bounds) const {
  if (has_frame()) {
    gfx::Rect window_bounds(
        [window_ frameRectForContentRect:bounds.ToCGRect()]);
    int frame_height = window_bounds.height() - bounds.height();
    window_bounds.set_y(window_bounds.y() - frame_height);
    return window_bounds;
  } else {
    return bounds;
  }
}

gfx::Rect NativeWindowMac::WindowBoundsToContentBounds(
    const gfx::Rect& bounds) const {
  if (has_frame()) {
    gfx::Rect content_bounds(
        [window_ contentRectForFrameRect:bounds.ToCGRect()]);
    int frame_height = bounds.height() - content_bounds.height();
    content_bounds.set_y(content_bounds.y() + frame_height);
    return content_bounds;
  } else {
    return bounds;
  }
}

void NativeWindowMac::UpdateWindowOriginalFrame() {
  original_frame_ = [window_ frame];
}

bool NativeWindowMac::HasStyleMask(NSUInteger flag) const {
  return [window_ styleMask] & flag;
}

void NativeWindowMac::SetStyleMask(bool on, NSUInteger flag) {
  // Changing the styleMask of a frameless windows causes it to change size so
  // we explicitly disable resizing while setting it.
  // ScopedDisableResize disable_resize;

  if (on) {
    [window_ setStyleMask:[window_ styleMask] | flag];
  } else {
    [window_ setStyleMask:[window_ styleMask] & (~flag)];
  }

  // Change style mask will make the zoom button revert to default, probably
  // a bug of Cocoa or macOS.
  SetMaximizable(maximizable_);
}

void NativeWindowMac::SetCollectionBehavior(bool on, NSUInteger flag) {
  if (on) {
    [window_ setCollectionBehavior:[window_ collectionBehavior] | flag];
  } else {
    [window_ setCollectionBehavior:[window_ collectionBehavior] & (~flag)];
  }

  // Change collectionBehavior will make the zoom button revert to default,
  // probably a bug of Cocoa or macOS.
  SetMaximizable(maximizable_);
}

void NativeWindowMac::SetWindowLevel(int unbounded_level) {
  int level = std::min(
      std::max(unbounded_level, CGWindowLevelForKey(kCGMinimumWindowLevelKey)),
      CGWindowLevelForKey(kCGMaximumWindowLevelKey));
  ui::ZOrderLevel z_order_level = level == NSNormalWindowLevel
                                      ? ui::ZOrderLevel::kNormal
                                      : ui::ZOrderLevel::kFloatingWindow;
  bool did_z_order_level_change = z_order_level != GetZOrderLevel();

  was_maximizable_ = IsMaximizable();

  z_order_ = z_order_level;
  [window_ setLevel:level];

  // Set level will make the zoom button revert to default, probably
  // a bug of Cocoa or macOS.
  SetMaximizable(was_maximizable_);

  // This must be notified at the very end or IsAlwaysOnTop
  // will not yet have been updated to reflect the new status
  if (did_z_order_level_change) {
    NativeWindow::NotifyWindowAlwaysOnTopChanged();
  }
}

void NativeWindowMac::InternalSetParentWindow(NativeWindow* parent,
                                              bool attach) {
  if (is_modal()) {
    return;
  }

  NativeWindow::SetParentWindow(parent);

  // // Do not remove/add if we are already properly attached.
  if (attach && parent &&
      [window_ parentWindow] == parent->GetNativeWindow().GetNativeNSWindow()) {
    return;
  }

  // Remove current parent window.
  if ([window_ parentWindow]) {
    [[window_ parentWindow] removeChildWindow:window_];
  }

  // Set new parent window.
  // Note that this method will force the window to become visible.
  if (parent && attach) {
    // Attaching a window as a child window resets its window level, so
    // save and restore it afterwards.
    NSInteger level = window_.level;
    [parent->GetNativeWindow().GetNativeNSWindow()
        addChildWindow:window_
               ordered:NSWindowAbove];
    [window_ setLevel:level];
  }
}

void NativeWindowMac::InternalSetWindowButtonVisibility(bool visible) {
  [[window_ standardWindowButton:NSWindowCloseButton] setHidden:!visible];
  [[window_ standardWindowButton:NSWindowMiniaturizeButton] setHidden:!visible];
  [[window_ standardWindowButton:NSWindowZoomButton] setHidden:!visible];
}

void NativeWindowMac::SetForwardMouseMessages(bool forward) {
  //[window_ setAcceptsMouseMovedEvents:forward];
}

std::string NativeWindowMac::GetAlwaysOnTopLevel() {
  std::string level_name = "normal";

  int level = [window_ level];
  if (level == NSFloatingWindowLevel) {
    level_name = "floating";
  } else if (level == NSTornOffMenuWindowLevel) {
    level_name = "torn-off-menu";
  } else if (level == NSModalPanelWindowLevel) {
    level_name = "modal-panel";
  } else if (level == NSMainMenuWindowLevel) {
    level_name = "main-menu";
  } else if (level == NSStatusWindowLevel) {
    level_name = "status";
  } else if (level == NSPopUpMenuWindowLevel) {
    level_name = "pop-up-menu";
  } else if (level == NSScreenSaverWindowLevel) {
    level_name = "screen-saver";
  }

  return level_name;
}

void NativeWindowMac::SetAlwaysOnTop(ui::ZOrderLevel z_order,
                                     const std::string& level_name,
                                     int relative_level) {
  if (z_order == ui::ZOrderLevel::kNormal) {
    SetWindowLevel(NSNormalWindowLevel);
    return;
  }

  int level = NSNormalWindowLevel;
  if (level_name == "floating") {
    level = NSFloatingWindowLevel;
  } else if (level_name == "torn-off-menu") {
    level = NSTornOffMenuWindowLevel;
  } else if (level_name == "modal-panel") {
    level = NSModalPanelWindowLevel;
  } else if (level_name == "main-menu") {
    level = NSMainMenuWindowLevel;
  } else if (level_name == "status") {
    level = NSStatusWindowLevel;
  } else if (level_name == "pop-up-menu") {
    level = NSPopUpMenuWindowLevel;
  } else if (level_name == "screen-saver") {
    level = NSScreenSaverWindowLevel;
  }

  SetWindowLevel(level + relative_level);
}

ui::ZOrderLevel NativeWindowMac::GetZOrderLevel() const {
  return z_order_;
}

void NativeWindowMac::SetActive(bool is_key) {
  is_active_ = is_key;
}

bool NativeWindowMac::IsActive() const {
  return is_active_;
}

void NativeWindowMac::SetWindowButtonVisibility(bool visible) {
  // window_button_visibility_ = visible;
  // if (buttons_proxy_) {
  //   if (visible)
  //     [buttons_proxy_ redraw];
  //   [buttons_proxy_ setVisible:visible];
  // }

  // if (title_bar_style_ != TitleBarStyle::kCustomButtonsOnHover)
  InternalSetWindowButtonVisibility(visible);

  // NotifyLayoutWindowControlsOverlay();
}

void NativeWindowMac::SetVibrancy(const std::string& type, int duration) {
  // TODO(Guo Xi): Add vibrancy support.
}

bool NativeWindowMac::GetWindowButtonVisibility() const {
  return ![window_ standardWindowButton:NSWindowZoomButton].hidden ||
         ![window_ standardWindowButton:NSWindowMiniaturizeButton].hidden ||
         ![window_ standardWindowButton:NSWindowCloseButton].hidden;
}

void NativeWindowMac::SetTrafficLightPosition(
    std::optional<gfx::Point> position) {
  // traffic_light_position_ = std::move(position);
  // if (buttons_proxy_) {
  //   [buttons_proxy_ setMargin:traffic_light_position_];
  //   NotifyLayoutWindowControlsOverlay();
  //}
}

std::optional<gfx::Point> NativeWindowMac::GetTrafficLightPosition() const {
  return traffic_light_position_;
}

void NativeWindowMac::RedrawTrafficLights() {
  // if (buttons_proxy_ && !IsFullscreen())
  //  [buttons_proxy_ redraw];
}

void NativeWindowMac::UpdateFrame() {
  NSRect fullscreenFrame = [[window_ screen] frame];
  [window_ setFrame:fullscreenFrame display:YES animate:YES];
}

gfx::NativeWindow NativeWindowMac::GetNativeWindow() const {
  return gfx::NativeWindow(window_);
}

void NativeWindowMac::SetBorderless(bool borderless) {
  SetStyleMask(!borderless, NSWindowStyleMaskTitled);
  SetStyleMask(borderless, NSWindowStyleMaskBorderless);
}

// static
NativeWindow* NativeWindow::Create(const gin_helper::Dictionary& options,
                                   NativeWindow* parent) {
  return new NativeWindowMac(options, parent);
}

}  // namespace lynxtron
