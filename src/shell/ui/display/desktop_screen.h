// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

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
