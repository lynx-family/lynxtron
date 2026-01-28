// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_UI_DISPLAY_DESKTOP_SCREEN_H_
#define LYNXTRON_SHELL_UI_DISPLAY_DESKTOP_SCREEN_H_

#include <memory>

namespace display {
class Screen;
}

namespace views {

// Creates a Screen that represents the screen of the environment that hosts
// a WindowTreeHost.
std::unique_ptr<display::Screen> CreateDesktopScreen();

}  // namespace views

#endif  // LYNXTRON_SHELL_UI_DISPLAY_DESKTOP_SCREEN_H_
