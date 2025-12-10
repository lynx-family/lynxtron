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
}
@property BOOL acceptsFirstMouse;
@property BOOL enableLargerThanScreen;
@property BOOL disableAutoHideCursor;
@property BOOL disableKeyOrMainWindow;
@property(nonatomic, retain) NSVisualEffectView* vibrantView;
@property(nonatomic, retain) NSImage* cornerMask;
- (id)initWithShell:(lynxtron::NativeWindowMac*)shell
          styleMask:(NSUInteger)styleMask;
- (lynxtron::NativeWindowMac*)shell;
//- (id)accessibilityFocusedUIElement;
- (NSRect)originalContentRectForFrameRect:(NSRect)frameRect;
- (void)toggleFullScreenMode:(id)sender;
- (NSImage*)_cornerMask;
@end

#endif  // LYNXTRON_SHELL_API_UI_MAC_LYNX_NS_WINDOW_H_
