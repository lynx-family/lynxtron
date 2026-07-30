// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_DISPLAY_WIN_SCREEN_WIN_DISPLAY_H_
#define LYNXTRON_SHELL_UI_DISPLAY_WIN_SCREEN_WIN_DISPLAY_H_

#include "shell/ui/display/display.h"
#include "shell/ui/gfx/geometry/rect.h"

namespace display {
namespace win {

class DisplayInfo;

// A display used by ScreenWin.
// It holds a display and additional parameters used for DPI calculations.
class ScreenWinDisplay final {
 public:
  ScreenWinDisplay();
  ScreenWinDisplay(const Display& display, const DisplayInfo& display_info);

  const Display& display() const { return display_; }
  const gfx::Rect& pixel_bounds() const { return pixel_bounds_; }

 private:
  Display display_;
  gfx::Rect pixel_bounds_;
};

}  // namespace win
}  // namespace display

#endif  // LYNXTRON_SHELL_UI_DISPLAY_WIN_SCREEN_WIN_DISPLAY_H_
