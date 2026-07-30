// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_DISPLAY_WIN_DISPLAY_INFO_H_
#define LYNXTRON_SHELL_UI_DISPLAY_WIN_DISPLAY_INFO_H_

#include <windows.h>

#include <stdint.h>

#include "shell/ui/display/display.h"

namespace display {
namespace win {

// Gathers the parameters necessary to create a win::ScreenWinDisplay.
class DisplayInfo final {
 public:
  DisplayInfo(const MONITORINFOEX& monitor_info,
              float device_scale_factor,
              Display::Rotation rotation,
              int display_frequency);
  ~DisplayInfo();

  static int64_t DeviceIdFromDeviceName(const wchar_t* device_name);

  int64_t id() const { return id_; }
  const gfx::Rect& screen_rect() const { return screen_rect_; }
  const gfx::Rect& screen_work_rect() const { return screen_work_rect_; }
  float device_scale_factor() const { return device_scale_factor_; }
  Display::Rotation rotation() const { return rotation_; }
  int display_frequency() const { return display_frequency_; }

 private:
  int64_t id_;
  gfx::Rect screen_rect_;
  gfx::Rect screen_work_rect_;
  float device_scale_factor_;
  Display::Rotation rotation_;
  int display_frequency_;
};

}  // namespace win
}  // namespace display

#endif  // LYNXTRON_SHELL_UI_DISPLAY_WIN_DISPLAY_INFO_H_
