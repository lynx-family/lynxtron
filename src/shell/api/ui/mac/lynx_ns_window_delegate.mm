// Copyright (c) 2018 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/ui/mac/lynx_ns_window_delegate.h"

#include <algorithm>

#include "base/mac/mac_util.h"
#include "base/task/single_thread_task_runner.h"
#include "shell/api/native_window_mac.h"
#include "shell/api/ui/mac/lynx_ns_window.h"
#include "shell/app/application.h"
#include "shell/ui/gfx/geometry/resize_utils.h"
#include "shell/ui/gfx/mac/coordinate_conversion.h"

using FullScreenTransitionState =
    lynxtron::NativeWindowMac::FullScreenTransitionState;

@implementation LynxNSWindowDelegate

namespace {

constexpr CGFloat kResizeEdgeDetectionInset = 8.0;

gfx::ResizeEdge EdgeFromWindowPoint(NSWindow* window, NSPoint point_in_window) {
  const NSRect frame = [window frame];
  const CGFloat w = frame.size.width;
  const CGFloat h = frame.size.height;

  const bool near_left = point_in_window.x <= kResizeEdgeDetectionInset;
  const bool near_right = (w - point_in_window.x) <= kResizeEdgeDetectionInset;
  const bool near_bottom = point_in_window.y <= kResizeEdgeDetectionInset;
  const bool near_top = (h - point_in_window.y) <= kResizeEdgeDetectionInset;

  if (near_left && near_bottom) {
    return gfx::ResizeEdge::kBottomLeft;
  }
  if (near_right && near_bottom) {
    return gfx::ResizeEdge::kBottomRight;
  }
  if (near_left && near_top) {
    return gfx::ResizeEdge::kTopLeft;
  }
  if (near_right && near_top) {
    return gfx::ResizeEdge::kTopRight;
  }
  if (near_left) {
    return gfx::ResizeEdge::kLeft;
  }
  if (near_right) {
    return gfx::ResizeEdge::kRight;
  }
  if (near_top) {
    return gfx::ResizeEdge::kTop;
  }
  return gfx::ResizeEdge::kBottom;
}

bool IsCornerEdge(gfx::ResizeEdge edge) {
  switch (edge) {
    case gfx::ResizeEdge::kTopLeft:
    case gfx::ResizeEdge::kTopRight:
    case gfx::ResizeEdge::kBottomLeft:
    case gfx::ResizeEdge::kBottomRight:
      return true;
    default:
      return false;
  }
}

bool IsHorizontalEdge(gfx::ResizeEdge edge) {
  switch (edge) {
    case gfx::ResizeEdge::kLeft:
    case gfx::ResizeEdge::kRight:
      return true;
    default:
      return false;
  }
}

bool IsVerticalEdge(gfx::ResizeEdge edge) {
  switch (edge) {
    case gfx::ResizeEdge::kTop:
    case gfx::ResizeEdge::kBottom:
      return true;
    default:
      return false;
  }
}

}  // namespace

- (id)initWithShell:(lynxtron::NativeWindowMac*)shell {
  if (self = [super init]) {
    shell_ = shell;
    is_zooming_ = false;
    level_ = [shell_->GetNativeWindow().GetNativeNSWindow() level];
  }
  return self;
}

- (void)cleanup {
  shell_ = nullptr;
}

#pragma mark - NSWindowDelegate

// Called when the user clicks the zoom button or selects it from the Window
// menu to determine the "standard size" of the window.
- (NSRect)windowWillUseStandardFrame:(NSWindow*)window
                        defaultFrame:(NSRect)frame {
  if (shell_->GetAspectRatio() > 0.0) {
    shell_->set_default_frame_for_zoom(frame);
  }

  return frame;
}

- (void)windowDidBecomeMain:(NSNotification*)notification {
  shell_->NotifyWindowFocus();
  shell_->RedrawTrafficLights();
}

- (void)windowDidResignMain:(NSNotification*)notification {
  shell_->NotifyWindowBlur();
  shell_->RedrawTrafficLights();
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  shell_->NotifyWindowIsKeyChanged(true);
  shell_->RedrawTrafficLights();
}

- (void)windowDidResignKey:(NSNotification*)notification {
  // If our app is still active and we're still the key window, ignore this
  // message, since it just means that a menu extra (on the "system status bar")
  // was activated; we'll get another |-windowDidResignKey| if we ever really
  // lose key window status.
  if ([NSApp isActive] && ([NSApp keyWindow] == [notification object])) {
    return;
  }

  shell_->NotifyWindowIsKeyChanged(false);
  shell_->RedrawTrafficLights();
}

- (NSSize)windowWillResize:(NSWindow*)sender toSize:(NSSize)frameSize {
  NSSize newSize = frameSize;
  double aspectRatio = shell_->GetAspectRatio();
  NSWindow* window = shell_->GetNativeWindow().GetNativeNSWindow();

  if (!resizing_edge_) {
    // Try to detect the active resize edge from the current event location.
    // If this fails (e.g. reduce motion / event stream mismatch), fall back to
    // the legacy right/bottom heuristic below.
    NSEvent* event = [NSApp currentEvent];
    if (event && event.window == sender) {
      resizing_edge_ = EdgeFromWindowPoint(sender, [event locationInWindow]);
    }
  }

  if (aspectRatio > 0.0) {
    gfx::Size windowSize = shell_->GetSize();
    gfx::Size contentSize = shell_->GetContentSize();
    gfx::Size extraSize = shell_->GetAspectRatioExtraSize();

    double extraWidthPlusFrame =
        windowSize.width() - contentSize.width() + extraSize.width();
    double extraHeightPlusFrame =
        windowSize.height() - contentSize.height() + extraSize.height();

    // Decide which dimension to treat as the "driving" dimension based on the
    // resize edge. For corner drags, fall back to the delta heuristic.
    bool drive_width = false;
    if (resizing_edge_) {
      if (IsHorizontalEdge(*resizing_edge_)) {
        drive_width = true;
      } else if (IsVerticalEdge(*resizing_edge_)) {
        drive_width = false;
      } else if (IsCornerEdge(*resizing_edge_)) {
        if (!resizingHorizontally_) {
          const auto widthDelta = frameSize.width - [window frame].size.width;
          const auto heightDelta =
              frameSize.height - [window frame].size.height;
          resizingHorizontally_ = std::abs(widthDelta) > std::abs(heightDelta);
        }
        drive_width = resizingHorizontally_.value_or(false);
      }
    }

    if (drive_width) {
      newSize.width = frameSize.width;
      newSize.height =
          roundf((newSize.width - extraWidthPlusFrame) / aspectRatio +
                 extraHeightPlusFrame);
    } else {
      newSize.height = frameSize.height;
      newSize.width =
          roundf((newSize.height - extraHeightPlusFrame) * aspectRatio +
                 extraWidthPlusFrame);
    }
  }

  if (!resizingHorizontally_) {
    const auto widthDelta = frameSize.width - [window frame].size.width;
    const auto heightDelta = frameSize.height - [window frame].size.height;
    resizingHorizontally_ = std::abs(widthDelta) > std::abs(heightDelta);
  }

  {
    bool prevent_default = false;
    NSRect new_bounds = NSMakeRect(sender.frame.origin.x, sender.frame.origin.y,
                                   newSize.width, newSize.height);
    shell_->NotifyWindowWillResize(
        gfx::ScreenRectFromNSRect(new_bounds),
        resizing_edge_.value_or(*resizingHorizontally_
                                    ? gfx::ResizeEdge::kRight
                                    : gfx::ResizeEdge::kBottom),
        prevent_default);
    if (prevent_default) {
      return sender.frame.size;
    }
  }

  return newSize;
}

- (void)windowDidResize:(NSNotification*)notification {
  //[super windowDidResize:notification];
  shell_->NotifyWindowResize();
  shell_->RedrawTrafficLights();
}

- (void)windowWillMove:(NSNotification*)notification {
  // NSWindowDelegate's windowWillMove: does not provide a way to cancel.
  // For cancellable will-move semantics, LynxNSWindow overrides
  // -setFrameOrigin: and dispatches NotifyWindowWillMove() for user-initiated
  // drags.
}

- (void)windowDidMove:(NSNotification*)notification {
  shell_->NotifyWindowMove();
  shell_->NotifyWindowMoved();
}

- (void)windowWillMiniaturize:(NSNotification*)notification {
  if (!shell_) {
    return;
  }
  NSWindow* window = shell_->GetNativeWindow().GetNativeNSWindow();
  // store the current status window level to be restored in
  // windowDidDeminiaturize
  level_ = [window level];
  shell_->SetWindowLevel(NSNormalWindowLevel);
  shell_->UpdateWindowOriginalFrame();
  shell_->DetachChildren();
  // Hide the traffic light buttons container before miniaturize so that when
  // the window is restored, macOS does not render the buttons at their default
  // position during the deminiaturize animation.
  shell_->HideTrafficLights();
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
  if (!shell_) {
    return;
  }
  shell_->NotifyWindowMinimize();
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
  if (!shell_) {
    return;
  }
  shell_->SetWindowLevel(level_);
  shell_->AttachChildren();
  // Reposition traffic light buttons and make them visible again. They were
  // hidden in windowWillMiniaturize to prevent a flash at the default (0,0)
  // position during the restore animation.
  shell_->RestoreTrafficLights();
  shell_->NotifyWindowRestore();
}

- (BOOL)windowShouldZoom:(NSWindow*)window toFrame:(NSRect)newFrame {
  is_zooming_ = true;
  return YES;
}

- (void)windowDidEndLiveResize:(NSNotification*)notification {
  resizingHorizontally_.reset();
  resizing_edge_.reset();
  shell_->NotifyWindowResized();
  if (is_zooming_) {
    if (shell_->IsMaximized()) {
      shell_->NotifyWindowMaximize();
    } else if (shell_->consume_restore_from_maximize_pending()) {
      shell_->NotifyWindowRestore();
    } else {
      shell_->NotifyWindowUnmaximize();
    }
    is_zooming_ = false;
  }
}

- (void)windowWillEnterFullScreen:(NSNotification*)notification {
  // Store resizable mask so it can be restored after exiting fullscreen.
  is_resizable_ = shell_->HasStyleMask(NSWindowStyleMaskResizable);

  // Keep transition state in sync with NativeWindowMac::SetFullScreen().
  shell_->set_fullscreen_transition_state(FullScreenTransitionState::ENTERING);
  shell_->NotifyWindowWillEnterFullScreen();

  // Set resizable to true before entering fullscreen.
  shell_->SetResizable(true);
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
  shell_->set_fullscreen_transition_state(FullScreenTransitionState::NONE);
  shell_->NotifyWindowEnterFullScreen();

  if (shell_->HandleDeferredClose()) {
    return;
  }
  shell_->HandlePendingFullscreenTransitions();
}

- (void)windowWillExitFullScreen:(NSNotification*)notification {
  shell_->set_fullscreen_transition_state(FullScreenTransitionState::EXITING);

  shell_->NotifyWindowWillLeaveFullScreen();
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
  shell_->set_fullscreen_transition_state(FullScreenTransitionState::NONE);

  shell_->SetResizable(is_resizable_);
  shell_->NotifyWindowLeaveFullScreen();

  if (shell_->HandleDeferredClose()) {
    return;
  }

  shell_->HandlePendingFullscreenTransitions();
}

- (void)windowWillClose:(NSNotification*)notification {
  if (!shell_) {
    return;
  }
  shell_->NotifyWindowClosed();

  // Something called -[NSWindow close] on a sheet rather than calling
  // -[NSWindow endSheet:] on its parent. If the modal session is not ended
  // then the parent will never be able to show another sheet. But calling
  // -endSheet: here will block the thread with an animation, so post a task.
  if (shell_->is_modal() && shell_->parent() && shell_->IsVisible()) {
    NSWindow* window = shell_->GetNativeWindow().GetNativeNSWindow();
    NSWindow* sheetParent = [window sheetParent];
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          [sheetParent endSheet:window];
        }));
  }

  // Clear delegate early. On some macOS versions, delegate callbacks can be
  // delivered after the window has begun closing.
  NSWindow* window = notification.object;
  if ([window isKindOfClass:[LynxNSWindow class]]) {
    [(LynxNSWindow*)window cleanup];
  }
  [self cleanup];
  [window setDelegate:nil];
}

- (BOOL)windowShouldClose:(id)window {
  shell_->NotifyWindowCloseButtonClicked();
  return NO;
}

- (NSRect)window:(NSWindow*)window
    willPositionSheet:(NSWindow*)sheet
            usingRect:(NSRect)rect {
  NSView* view = window.contentView;

  rect.origin.x = shell_->GetSheetOffsetX();
  rect.origin.y = view.frame.size.height - shell_->GetSheetOffsetY();
  return rect;
}

- (void)windowWillBeginSheet:(NSNotification*)notification {
  shell_->NotifyWindowSheetBegin();
}

- (void)windowDidEndSheet:(NSNotification*)notification {
  shell_->NotifyWindowSheetEnd();
}

- (IBAction)newWindowForTab:(id)sender {
  shell_->NotifyWindowNewWindowForTab();
  lynxtron::Application::Get()->NewWindowForTab();
}

@end
