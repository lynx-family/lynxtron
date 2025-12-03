// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shell/ui/display/win/desktop_screen_win.h"

#include <memory>

#include "shell/ui/display/desktop_screen.h"
#include "shell/ui/display/screen.h"

namespace views {

DesktopScreenWin::DesktopScreenWin() {
  // DCHECK(!display::Screen::HasScreen());
  display::Screen::SetScreenInstance(this);
}

DesktopScreenWin::~DesktopScreenWin() {
  display::Screen::SetScreenInstance(nullptr);
}

////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<display::Screen> CreateDesktopScreen() {
  return std::make_unique<DesktopScreenWin>();
}

}  // namespace views
