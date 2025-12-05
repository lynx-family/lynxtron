// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef PLUGIN_CEF_DARWIN_MACOS_CEF_WEBVIEW_MAC_H_
#define PLUGIN_CEF_DARWIN_MACOS_CEF_WEBVIEW_MAC_H_

#import <AppKit/AppKit.h>

#include "plugin/cef/cef_webview.h"
#include "plugin/cef/darwin/macos/CEFWebviewClientMac.h"

namespace lynxtron {
namespace plugin {

class CEFWebviewMac : public CEFWebview {
 public:
  CEFWebviewMac(lynx_view_t* lynx_view) : CEFWebview(lynx_view) {}

  // LynxNativeView
  void OnAttach() override;
  void OnDetach() override;
  void OnDestroy() override;
  void OnLayoutChanged(float left,
                       float top,
                       float width,
                       float height,
                       float pixel_ratio) override;

  float GetPixelRatio() const override;
  void SetupClient() override;
  void OnMouseWheelEvent(int x,
                         int y,
                         int modifiers,
                         double delta_x,
                         double delta_y) override;
  void OnMouseMoveEvent(int x, int y, int modifiers, bool mouse_leave) override;
  void OnMouseClickEvent(int x, int y, int buttons, bool mouse_up) override;

  void AddSubview(NSView* view);

  bool IsAttached() const { return attached_; }

 private:
  friend CEFWebviewClientMac;
  NSRect bounds_;
  bool attached_ = false;

  int last_click_x_ = 0;
  int last_click_y_ = 0;
  int last_click_button_ = 0;
  int last_click_count_ = 1;
  double last_click_time_ = 0;
};

}  // namespace plugin
}  // namespace lynxtron

#endif  // PLUGIN_CEF_DARWIN_MACOS_CEF_WEBVIEW_MAC_H_
