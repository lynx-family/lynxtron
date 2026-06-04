// Copyright (c) 2018 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_UI_MAC_LYNX_NS_WINDOW_DELEGATE_H_
#define LYNXTRON_SHELL_API_UI_MAC_LYNX_NS_WINDOW_DELEGATE_H_

#import <AppKit/AppKit.h>

#include "base/memory/raw_ptr.h"
#include "shell/api/native_window_mac.h"
#include "shell/ui/gfx/geometry/resize_utils.h"

namespace lynxtron {
class NativeWindowMac;
}

@interface LynxNSWindowDelegate : NSObject <NSWindowDelegate> {
 @private
  raw_ptr<lynxtron::NativeWindowMac> shell_;
  bool is_zooming_;
  int level_;
  bool is_resizable_;

  // Only valid during a live resize.
  // Used to keep track of which edge is being resized, and to derive whether a
  // corner resize is primarily horizontal or vertical.
  std::optional<gfx::ResizeEdge> resizing_edge_;
  std::optional<bool> resizingHorizontally_;
}
- (id)initWithShell:(lynxtron::NativeWindowMac*)shell;
- (void)cleanup;
@end

#endif  // LYNXTRON_SHELL_API_UI_MAC_LYNX_NS_WINDOW_DELEGATE_H_
