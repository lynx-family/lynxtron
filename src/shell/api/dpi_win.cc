// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/api/dpi_win.h"

#include <limits.h>

#include "base/check.h"
#include "shell/ui/display/win/screen_win.h"
#include "shell/ui/gfx/geometry/size.h"

namespace lynxtron {
namespace {

gfx::Size ScaleWindowSizePreservingUnbounded(const gfx::Size& size,
                                             float scale_factor) {
  // ScaleToRoundedSize cannot safely scale INT_MAX. Substitute a bounded value
  // during the arithmetic and restore each unbounded dimension afterwards.
  const gfx::Size bounded_size(size.width() == INT_MAX ? 0 : size.width(),
                               size.height() == INT_MAX ? 0 : size.height());
  gfx::Size scaled_size = gfx::ScaleToRoundedSize(bounded_size, scale_factor);
  if (size.width() == INT_MAX) {
    scaled_size.set_width(INT_MAX);
  }
  if (size.height() == INT_MAX) {
    scaled_size.set_height(INT_MAX);
  }
  return scaled_size;
}

float GetScaleFactorForPixelRect(HWND hwnd, const gfx::Rect& pixel_bounds) {
  if (hwnd) {
    return display::win::ScreenWin::GetScaleFactorForHWND(hwnd);
  }
  return display::win::ScreenWin::GetScaleFactorForScreenRect(pixel_bounds);
}

float GetScaleFactorForDIPRect(HWND hwnd, const gfx::Rect& dip_bounds) {
  if (hwnd) {
    return display::win::ScreenWin::GetScaleFactorForHWND(hwnd);
  }
  return display::win::ScreenWin::GetScaleFactorForDIPRect(dip_bounds);
}

}  // namespace

gfx::Rect ScreenToDIPRectForWindow(HWND hwnd, const gfx::Rect& pixel_bounds) {
  const float scale_factor = GetScaleFactorForPixelRect(hwnd, pixel_bounds);
  const gfx::Point dip_origin =
      display::win::ScreenWin::ScreenToDIPRect(hwnd, pixel_bounds).origin();
  const gfx::Size dip_size = ScaleWindowSizePreservingUnbounded(
      pixel_bounds.size(), 1.0f / scale_factor);
  return gfx::Rect(dip_origin, dip_size);
}

gfx::Rect DIPToScreenRectForWindow(HWND hwnd, const gfx::Rect& dip_bounds) {
  const float scale_factor = GetScaleFactorForDIPRect(hwnd, dip_bounds);
  const gfx::Point pixel_origin =
      display::win::ScreenWin::DIPToScreenRect(hwnd, dip_bounds).origin();
  const gfx::Size pixel_size =
      ScaleWindowSizePreservingUnbounded(dip_bounds.size(), scale_factor);
  return gfx::Rect(pixel_origin, pixel_size);
}

gfx::Size DIPToScreenSizeForWindow(HWND hwnd, const gfx::Size& dip_size) {
  return ScaleWindowSizePreservingUnbounded(
      dip_size, display::win::ScreenWin::GetScaleFactorForHWND(hwnd));
}

gfx::Size ScreenToDIPSizeForWindow(HWND hwnd, const gfx::Size& pixel_size) {
  return ScaleWindowSizePreservingUnbounded(
      pixel_size, 1.0f / display::win::ScreenWin::GetScaleFactorForHWND(hwnd));
}

gfx::Rect UpdateScreenRectForWindow(const gfx::Rect& pixel_bounds,
                                    const std::optional<gfx::Point>& dip_origin,
                                    const std::optional<gfx::Size>& dip_size) {
  DCHECK(dip_origin || dip_size);

  if (dip_origin && dip_size) {
    // A complete target rect may cross displays, so select the scale factor
    // from that target rather than from the window's current HWND.
    return DIPToScreenRectForWindow(nullptr, gfx::Rect(*dip_origin, *dip_size));
  }

  // For a partial update, keep the untouched field on the physical-pixel grid.
  // Reconstructing the whole rect through integer DIP can move or resize it by
  // one pixel at fractional scale factors.
  gfx::Rect updated_pixel_bounds(pixel_bounds);
  if (dip_origin) {
    updated_pixel_bounds.set_origin(
        DIPToScreenRectForWindow(nullptr, gfx::Rect(*dip_origin, gfx::Size()))
            .origin());
  }
  if (dip_size) {
    updated_pixel_bounds.set_size(ScaleWindowSizePreservingUnbounded(
        *dip_size, GetScaleFactorForPixelRect(nullptr, pixel_bounds)));
  }
  return updated_pixel_bounds;
}

}  // namespace lynxtron
