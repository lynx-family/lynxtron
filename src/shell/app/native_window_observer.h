// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_APP_NATIVE_WINDOW_OBSERVER_H_
#define LYNXTRON_SHELL_APP_NATIVE_WINDOW_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"
#include "base/values.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

class GURL;

namespace gfx {
class Rect;
enum class ResizeEdge;
}  // namespace gfx

namespace lynxtron {

class NativeWindowObserver : public base::CheckedObserver {
 public:
  ~NativeWindowObserver() override = default;

  // Called when user is starting an navigation in web page.
  virtual void WillNavigate(bool& prevent_default, const GURL& url) {}

  // Called when the window is gonna closed.
  virtual void WillCloseWindow(bool& prevent_default) {}

  // Called when the window is closed.
  virtual void OnWindowClosed() {}

  // Called when Windows sends WM_ENDSESSION message
  virtual void OnWindowEndSession() {}

  // Called when window loses focus.
  virtual void OnWindowBlur() {}

  // Called when window gains focus.
  virtual void OnWindowFocus() {}

  // Called when window gained or lost key window status.
  virtual void OnWindowIsKeyChanged(bool is_key) {}

  // Called when window is shown.
  virtual void OnWindowShow() {}

  // Called when window is hidden.
  virtual void OnWindowHide() {}

  // Called when window state changed.
  virtual void OnWindowMaximize() {}
  virtual void OnWindowUnmaximize() {}
  virtual void OnWindowMinimize() {}
  virtual void OnWindowRestore() {}
  virtual void OnWindowWillResize(const gfx::Rect& new_bounds,
                                  gfx::ResizeEdge edge,
                                  bool& prevent_default) {}
  virtual void OnWindowResize() {}
  virtual void OnWindowResized() {}
  virtual void OnWindowWillMove(const gfx::Rect& new_bounds,
                                bool& prevent_default) {}
  virtual void OnWindowMove() {}
  virtual void OnWindowMoved() {}
  virtual void OnWindowScrollTouchBegin() {}
  virtual void OnWindowScrollTouchEnd() {}
  virtual void OnWindowSwipe(const std::string& direction) {}
  virtual void OnWindowRotateGesture(float rotation) {}
  virtual void OnWindowInputEvent(const base::Value::Dict& details) {}
  virtual void OnWindowSheetBegin() {}
  virtual void OnWindowSheetEnd() {}
  virtual void OnWindowWillEnterFullScreen() {}
  virtual void OnWindowWillLeaveFullScreen() {}
  virtual void OnWindowEnterFullScreen() {}
  virtual void OnWindowLeaveFullScreen() {}
  virtual void OnWindowAlwaysOnTopChanged() {}
  virtual void OnTouchBarItemResult(const std::string& item_id,
                                    const base::Value::Dict& details) {}
  virtual void OnNewWindowForTab() {}
  virtual void OnSystemContextMenu(int x, int y, bool& prevent_default) {}

// Called when window message received
#if BUILDFLAG(IS_WIN)
  virtual void OnWindowMessage(UINT message, WPARAM w_param, LPARAM l_param) {}
#endif

  // Called on Windows when App Commands arrive (WM_APPCOMMAND)
  // Some commands are implemented on on other platforms as well
  virtual void OnExecuteAppCommand(std::string_view command_name) {}
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_NATIVE_WINDOW_OBSERVER_H_
