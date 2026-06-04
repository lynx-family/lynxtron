// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_API_UI_MAC_LYNX_NS_WINDOW_H_
#define LYNXTRON_SHELL_API_UI_MAC_LYNX_NS_WINDOW_H_

#import <AppKit/AppKit.h>

#include "base/memory/raw_ptr.h"
#include "shell/api/native_window_mac.h"

namespace lynxtron {

class NativeWindowMac;

}  // namespace lynxtron

@interface LynxNSWindow : NSWindow {
 @private
  raw_ptr<lynxtron::NativeWindowMac> shell_;
  // Used to make will-move cancellable for user-initiated drags.
  bool suppress_set_frame_origin_;
  bool has_last_allowed_origin_;
  NSPoint last_allowed_origin_;
  bool in_move_drag_;
  bool cached_move_prevent_default_;
}
@property BOOL enableLargerThanScreen;
@property BOOL disableAutoHideCursor;
@property BOOL disableKeyOrMainWindow;
@property BOOL customButtonsOnHover;
@property(nonatomic, retain) NSVisualEffectView* vibrantView;
- (id)initWithShell:(lynxtron::NativeWindowMac*)shell
          styleMask:(NSUInteger)styleMask;
- (void)cleanup;
- (lynxtron::NativeWindowMac*)shell;
//- (id)accessibilityFocusedUIElement;
- (NSRect)originalContentRectForFrameRect:(NSRect)frameRect;
- (void)toggleFullScreenMode:(id)sender;
@end

#endif  // LYNXTRON_SHELL_API_UI_MAC_LYNX_NS_WINDOW_H_
