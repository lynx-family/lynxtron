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
#include "shell/ui/gfx/geometry/resize_utils.h"
#include "shell/ui/gfx/mac/coordinate_conversion.h"

using FullScreenTransitionState =
    lynxtron::NativeWindowMac::FullScreenTransitionState;

@implementation LynxNSWindowDelegate

- (id)initWithShell:(lynxtron::NativeWindowMac*)shell {
  if (self = [super init]) {
    shell_ = shell;
    is_zooming_ = false;
    level_ = [shell_->GetNativeWindow().GetNativeNSWindow() level];
  }
  return self;
}

#pragma mark - NSWindowDelegate

- (void)windowDidChangeOcclusionState:(NSNotification*)notification {
  // notification.object is the window that changed its state.
  // It's safe to use self.window instead if you don't assign one delegate to
  // many windows
  NSWindow* window = notification.object;

  // check occlusion binary flag
  if (window.occlusionState & NSWindowOcclusionStateVisible) {
    // The app is visible
    shell_->NotifyWindowShow();
  } else {
    // The app is not visible
    shell_->NotifyWindowHide();
  }
}

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

  if (aspectRatio > 0.0) {
    gfx::Size windowSize = shell_->GetSize();
    // TODO(Guo Xi): support content size.
    // gfx::Size contentSize = shell_->GetContentSize();
    gfx::Size extraSize = shell_->GetAspectRatioExtraSize();

    double extraWidthPlusFrame =
        windowSize.width() /*- contentSize.width() */ + extraSize.width();
    double extraHeightPlusFrame =
        windowSize.height() /*- contentSize.height() */ + extraSize.height();

    newSize.width =
        roundf((frameSize.height - extraHeightPlusFrame) * aspectRatio +
               extraWidthPlusFrame);
    newSize.height =
        roundf((newSize.width - extraWidthPlusFrame) / aspectRatio +
               extraHeightPlusFrame);
  }

  if (!resizingHorizontally_) {
    // NSWindow* window = shell_->GetNativeWindow().GetNativeNSWindow();
    // const auto widthDelta = frameSize.width - [window frame].size.width;
    // const auto heightDelta = frameSize.height - [window frame].size.height;
    // resizingHorizontally_ = std::abs(widthDelta) > std::abs(heightDelta);
  }

  {
    bool prevent_default = false;
    // NSRect new_bounds = NSMakeRect(sender.frame.origin.x,
    // sender.frame.origin.y,
    //                               newSize.width, newSize.height);
    // shell_->NotifyWindowWillResize(gfx::ScreenRectFromNSRect(new_bounds),
    //                                *resizingHorizontally_
    //                                    ? gfx::ResizeEdge::kRight
    //                                    : gfx::ResizeEdge::kBottom,
    //                                prevent_default);
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
  NSWindow* window = [notification object];
  NSSize size = [[window contentView] frame].size;
  NSRect new_bounds = NSMakeRect(window.frame.origin.x, window.frame.origin.y,
                                 size.width, size.height);
  bool prevent_default = false;

  // prevent_default has no effect
  shell_->NotifyWindowWillMove(gfx::ScreenRectFromNSRect(new_bounds),
                               prevent_default);
}

- (void)windowDidMove:(NSNotification*)notification {
  //[super windowDidMove:notification];
  // TODO(zcbenz): Remove the alias after figuring out a proper
  // way to dispatch move.
  shell_->NotifyWindowMove();
  shell_->NotifyWindowMoved();
}

- (void)windowWillMiniaturize:(NSNotification*)notification {
  NSWindow* window = shell_->GetNativeWindow().GetNativeNSWindow();
  // store the current status window level to be restored in
  // windowDidDeminiaturize
  level_ = [window level];
  shell_->SetWindowLevel(NSNormalWindowLevel);
  shell_->UpdateWindowOriginalFrame();
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
  //[super windowDidMiniaturize:notification];
  shell_->NotifyWindowMinimize();
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
  //[super windowDidDeminiaturize:notification];
  shell_->SetWindowLevel(level_);
  shell_->NotifyWindowRestore();
}

- (BOOL)windowShouldZoom:(NSWindow*)window toFrame:(NSRect)newFrame {
  is_zooming_ = true;
  return YES;
}

- (void)windowDidEndLiveResize:(NSNotification*)notification {
  resizingHorizontally_.reset();
  shell_->NotifyWindowResized();
  if (is_zooming_) {
    if (shell_->IsMaximized()) {
      shell_->NotifyWindowMaximize();
    } else {
      shell_->NotifyWindowUnmaximize();
    }
    is_zooming_ = false;
  }
}

- (void)windowWillEnterFullScreen:(NSNotification*)notification {
  // Store resizable mask so it can be restored after exiting fullscreen.
  is_resizable_ = shell_->HasStyleMask(NSWindowStyleMaskResizable);

  // shell_->set_fullscreen_transition_state(FullScreenTransitionState::ENTERING);

  // shell_->NotifyWindowWillEnterFullScreen();

  // Set resizable to true before entering fullscreen.
  shell_->SetResizable(true);
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
  shell_->set_fullscreen_transition_state(FullScreenTransitionState::NONE);
  shell_->NotifyWindowEnterFullScreen();
  shell_->HandlePendingFullscreenTransitions();
}

- (void)windowWillExitFullScreen:(NSNotification*)notification {
  shell_->set_fullscreen_transition_state(FullScreenTransitionState::EXITING);

  // shell_->NotifyWindowWillLeaveFullScreen();
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
  shell_->set_fullscreen_transition_state(FullScreenTransitionState::NONE);

  shell_->SetResizable(is_resizable_);
  shell_->NotifyWindowLeaveFullScreen();

  // if (shell_->HandleDeferredClose())
  //  return;

  shell_->HandlePendingFullscreenTransitions();
}

- (void)windowWillClose:(NSNotification*)notification {
  // shell_->Cleanup();
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

  // Clears the delegate when window is going to be closed, since EL Capitan it
  // is possible that the methods of delegate would get called after the window
  // has been closed.
  // auto* bridge_host =
  // views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
  //    shell_->GetNativeWindow());
  // auto* bridged_view = bridge_host->GetInProcessNSWindowBridge();
  // bridged_view->OnWindowWillClose();
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
  // shell_->NotifyNewWindowForTab();
  // electron::Application::Get()->NewWindowForTab();
}

// #pragma mark - NSTouchBarDelegate

// - (NSTouchBarItem*)touchBar:(NSTouchBar*)touchBar
//       makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier {
//   if (touchBar && shell_->touch_bar())
//     return [shell_->touch_bar() makeItemForIdentifier:identifier];
//   else
//     return nil;
// }

// #pragma mark - QLPreviewPanelDataSource

// - (NSInteger)numberOfPreviewItemsInPreviewPanel:(QLPreviewPanel*)panel {
//   return 1;
// }

// - (id<QLPreviewItem>)previewPanel:(QLPreviewPanel*)panel
//                previewItemAtIndex:(NSInteger)index {
//   return shell_->preview_item();
// }

@end
