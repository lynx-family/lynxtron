// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/dpi_win.h"

#include <limits.h>

#include <array>

#include "base/check.h"
#include "shell/ui/display/win/display_info.h"
#include "shell/ui/display/win/screen_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lynxtron {
namespace {

// Avoid depending on the host machine's DPI setting. The injected 175% display
// exercises the fractional rounding behavior that originally exposed the
// window-size drift.
class FractionalScaleScreenWin final : public display::win::ScreenWin {
 public:
  FractionalScaleScreenWin() : ScreenWin(false) {
    MONITORINFOEX monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    const HMONITOR primary_monitor =
        ::MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    CHECK(::GetMonitorInfo(primary_monitor, &monitor_info));
    UpdateFromDisplayInfos({display::win::DisplayInfo(
        monitor_info, 1.75f, display::Display::ROTATE_0, 60)});
  }
};

class DPIWinTest : public testing::Test {
 protected:
  FractionalScaleScreenWin screen_;
};

TEST_F(DPIWinTest, RoundedWindowRectRoundTripsAtFractionalScale) {
  constexpr std::array<int, 8> kDimensions = {1,   2,   319, 320,
                                              399, 400, 401, 601};

  for (int width : kDimensions) {
    for (int height : kDimensions) {
      const gfx::Rect dip_bounds(13, 17, width, height);
      const gfx::Rect pixel_bounds =
          DIPToScreenRectForWindow(nullptr, dip_bounds);
      const gfx::Rect round_tripped_bounds =
          ScreenToDIPRectForWindow(nullptr, pixel_bounds);
      EXPECT_EQ(dip_bounds.x(), round_tripped_bounds.x());
      EXPECT_EQ(dip_bounds.y(), round_tripped_bounds.y());
      EXPECT_EQ(dip_bounds.width(), round_tripped_bounds.width());
      EXPECT_EQ(dip_bounds.height(), round_tripped_bounds.height());
    }
  }
}

TEST_F(DPIWinTest, WindowRectAndSizeUseIdenticalRounding) {
  const gfx::Size fixed_size(401, 399);
  const gfx::Size rect_size =
      DIPToScreenRectForWindow(nullptr, gfx::Rect(fixed_size)).size();
  const gfx::Size converted_size =
      DIPToScreenSizeForWindow(nullptr, fixed_size);
  EXPECT_EQ(gfx::Size(702, 698), converted_size);
  EXPECT_EQ(rect_size.width(), converted_size.width());
  EXPECT_EQ(rect_size.height(), converted_size.height());
}

TEST_F(DPIWinTest, PositionOnlyUpdatePreservesExactPixelSize) {
  const gfx::Rect current_pixel_bounds(701, 703, 701, 699);
  const gfx::Rect round_tripped_bounds = DIPToScreenRectForWindow(
      nullptr, ScreenToDIPRectForWindow(nullptr, current_pixel_bounds));
  ASSERT_NE(current_pixel_bounds.size(), round_tripped_bounds.size());

  const gfx::Rect updated_pixel_bounds = UpdateScreenRectForWindow(
      current_pixel_bounds, gfx::Point(13, 17), std::nullopt);

  EXPECT_EQ(current_pixel_bounds.size(), updated_pixel_bounds.size());
  EXPECT_NE(current_pixel_bounds.origin(), updated_pixel_bounds.origin());
}

TEST_F(DPIWinTest, SizeOnlyUpdatePreservesExactPixelOrigin) {
  const gfx::Rect current_pixel_bounds(701, 703, 701, 699);
  const gfx::Rect round_tripped_bounds = DIPToScreenRectForWindow(
      nullptr, ScreenToDIPRectForWindow(nullptr, current_pixel_bounds));
  ASSERT_NE(current_pixel_bounds.origin(), round_tripped_bounds.origin());

  const gfx::Rect updated_pixel_bounds = UpdateScreenRectForWindow(
      current_pixel_bounds, std::nullopt, gfx::Size(401, 399));

  EXPECT_EQ(current_pixel_bounds.origin(), updated_pixel_bounds.origin());
  EXPECT_EQ(gfx::Size(702, 698), updated_pixel_bounds.size());
}

TEST_F(DPIWinTest, FullBoundsUpdateUsesTargetRect) {
  const gfx::Rect current_pixel_bounds(701, 703, 701, 699);
  const gfx::Rect dip_bounds(13, 17, 401, 399);

  EXPECT_EQ(DIPToScreenRectForWindow(nullptr, dip_bounds),
            UpdateScreenRectForWindow(current_pixel_bounds, dip_bounds.origin(),
                                      dip_bounds.size()));
}

TEST_F(DPIWinTest, PreservesUnboundedConstraintDimensions) {
  const gfx::Size dip_size(INT_MAX, 399);
  const gfx::Size pixel_size = DIPToScreenSizeForWindow(nullptr, dip_size);
  EXPECT_EQ(INT_MAX, pixel_size.width());
  const gfx::Size round_tripped_size =
      ScreenToDIPSizeForWindow(nullptr, pixel_size);
  EXPECT_EQ(dip_size.width(), round_tripped_size.width());
  EXPECT_EQ(dip_size.height(), round_tripped_size.height());
}

}  // namespace
}  // namespace lynxtron
