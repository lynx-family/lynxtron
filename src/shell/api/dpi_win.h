// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_API_DPI_WIN_H_
#define LYNXTRON_SHELL_API_DPI_WIN_H_

#include <windows.h>

#include <optional>

#include "shell/ui/gfx/geometry/point_f.h"
#include "shell/ui/gfx/geometry/rect.h"

namespace lynxtron {

float GetScalingFactorFromDPI(int dpi);
int GetDefaultSystemDPI();
std::optional<MONITORINFOEX> GetMonitorInfoFromHMONITOR(HMONITOR monitor);
HMONITOR HMONITORFromWindow(HWND hwnd, DWORD default_options);
std::optional<MONITORINFOEX> MonitorInfoFromHMONITOR(HMONITOR monitor);
std::optional<MONITORINFOEX> MonitorInfoFromWindow(HWND hwnd,
                                                   DWORD default_options);
std::optional<int> GetPerMonitorDPI(HMONITOR monitor);
float GetMonitorScaleFactor(HMONITOR monitor);
int GetDPIForHWND(HWND hwnd);
gfx::PointF ScalePointRelative(const gfx::PointF& point,
                               const gfx::Point& from_origin,
                               const gfx::Point& to_origin,
                               const float scale_factor);
gfx::Rect ScreenToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds);
gfx::Rect DIPToScreenRect(HWND hwnd, const gfx::Rect& pixel_bounds);
gfx::Size DIPToScreenSize(HWND hwnd, const gfx::Size& dip_size);
gfx::Size ScreenToDIPSize(HWND hwnd, const gfx::Size& size_in_pixels);
}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_DPI_WIN_H_
