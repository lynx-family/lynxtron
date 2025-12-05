// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef PLUGIN_CEF_WIN_CEF_WEBVIEW_WIN_H_
#define PLUGIN_CEF_WIN_CEF_WEBVIEW_WIN_H_

#include <Windows.h>

#include "plugin/cef/cef_webview.h"

namespace lynxtron {
namespace plugin {

class CEFWebviewWin : public CEFWebview {
 public:
  CEFWebviewWin(lynx_view_t* lynx_view) : CEFWebview(lynx_view) {}

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

 private:
  // for Windows 7
  HWND GetOrCreateOwnedWin();
  void AdjustOwnedWinAndChild(HWND child);

  HWND wrapper_{nullptr};
  HWND win7_owned_win_{nullptr};
  RECT bounds_{0, 0, 0, 0};
  HWND hwnd_{nullptr};

  int last_click_x_ = 0;
  int last_click_y_ = 0;
  int last_click_button_ = 0;
  int last_click_count_ = 1;
  double last_click_time_ = 0;
};

}  // namespace plugin
}  // namespace lynxtron

#endif  // PLUGIN_CEF_WIN_CEF_WEBVIEW_WIN_H_
