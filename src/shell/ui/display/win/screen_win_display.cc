// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/ui/display/win/screen_win_display.h"

#include "shell/ui/display/win/display_info.h"

namespace display {
namespace win {

ScreenWinDisplay::ScreenWinDisplay() = default;

ScreenWinDisplay::ScreenWinDisplay(const Display& display,
                                   const DisplayInfo& display_info)
    : display_(display), pixel_bounds_(display_info.screen_rect()) {}
}  // namespace win
}  // namespace display
