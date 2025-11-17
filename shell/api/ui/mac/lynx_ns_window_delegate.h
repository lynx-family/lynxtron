// Copyright (c) 2018 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_API_UI_MAC_LYNX_NS_WINDOW_DELEGATE_H_
#define LYNXTRON_SHELL_API_UI_MAC_LYNX_NS_WINDOW_DELEGATE_H_

#import <AppKit/AppKit.h>

#include "base/memory/raw_ptr.h"
#include "shell/api/native_window_mac.h"

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
  // Used to keep track of whether a resize is happening horizontally or
  // vertically, even if physically the user is resizing in both directions.
  std::optional<bool> resizingHorizontally_;
}
- (id)initWithShell:(lynxtron::NativeWindowMac*)shell;
@end

#endif  // LYNXTRON_SHELL_API_UI_MAC_LYNX_NS_WINDOW_DELEGATE_H_
