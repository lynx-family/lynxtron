// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_UI_DISPLAY_WIN_DESKTOP_SCREEN_WIN_H_
#define LYNXTRON_SHELL_UI_DISPLAY_WIN_DESKTOP_SCREEN_WIN_H_

#include "shell/ui/display/win/screen_win.h"

namespace views {

class DesktopScreenWin : public display::win::ScreenWin {
 public:
  DesktopScreenWin();
  DesktopScreenWin(const DesktopScreenWin&) = delete;
  DesktopScreenWin& operator=(const DesktopScreenWin&) = delete;
  ~DesktopScreenWin() override;
};

}  // namespace views

#endif  // LYNXTRON_SHELL_UI_DISPLAY_WIN_DESKTOP_SCREEN_WIN_H_
