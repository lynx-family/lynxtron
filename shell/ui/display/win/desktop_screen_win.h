// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_WIN_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_WIN_H_

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

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_WIN_H_
