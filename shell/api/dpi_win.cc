#include "shell/api/dpi_win.h"

#include <shellscalingapi.h>
#include <windowsx.h>

#include "base/win/scoped_hdc.h"
#include "shell/ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace lynxtron {

const float kDefaultDPI = 96.f;

float GetScalingFactorFromDPI(int dpi) {
  return static_cast<float>(dpi) / kDefaultDPI;
}

int GetDefaultSystemDPI() {
  static int dpi_x = 0;
  static int dpi_y = 0;
  static bool should_initialize = true;

  if (should_initialize) {
    should_initialize = false;
    base::win::ScopedGetDC screen_dc(NULL);
    dpi_x = GetDeviceCaps(screen_dc, LOGPIXELSX);
    dpi_y = GetDeviceCaps(screen_dc, LOGPIXELSY);
    DCHECK_EQ(dpi_x, dpi_y);
  }
  return dpi_x;
}

std::optional<MONITORINFOEX> GetMonitorInfoFromHMONITOR(HMONITOR monitor) {
  MONITORINFOEX monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  if (::GetMonitorInfo(monitor, &monitor_info) == 0) {
    return std::nullopt;
  }
  return monitor_info;
}

HMONITOR HMONITORFromWindow(HWND hwnd, DWORD default_options) {
  return ::MonitorFromWindow(hwnd, default_options);
}

std::optional<MONITORINFOEX> MonitorInfoFromHMONITOR(HMONITOR monitor) {
  return GetMonitorInfoFromHMONITOR(monitor);
}

std::optional<MONITORINFOEX> MonitorInfoFromWindow(HWND hwnd,
                                                   DWORD default_options) {
  return MonitorInfoFromHMONITOR(HMONITORFromWindow(hwnd, default_options));
}

std::optional<int> GetPerMonitorDPI(HMONITOR monitor) {
  UINT dpi_x, dpi_y;
  if (!SUCCEEDED(
          ::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y))) {
    return std::nullopt;
  }

  DCHECK_EQ(dpi_x, dpi_y);
  return static_cast<int>(dpi_x);
}

float GetMonitorScaleFactor(HMONITOR monitor) {
  DCHECK(monitor);

  const auto dpi = GetPerMonitorDPI(monitor);
  return dpi ? GetScalingFactorFromDPI(dpi.value())
             : GetScalingFactorFromDPI(GetDefaultSystemDPI());
}

int GetDPIForHWND(HWND hwnd) {
  const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  return GetPerMonitorDPI(monitor).value_or(GetDefaultSystemDPI());
}

gfx::PointF ScalePointRelative(const gfx::PointF& point,
                               const gfx::Point& from_origin,
                               const gfx::Point& to_origin,
                               const float scale_factor) {
  const gfx::PointF relative_point = point - from_origin.OffsetFromOrigin();
  const gfx::PointF scaled_relative_point =
      gfx::ScalePoint(relative_point, scale_factor);
  return scaled_relative_point + to_origin.OffsetFromOrigin();
}

gfx::Rect ScreenToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds) {
  float scale_factor = GetScalingFactorFromDPI(GetDPIForHWND(hwnd));
  gfx::Rect dip_rect = ScaleToRoundedRect(pixel_bounds, 1.0f / scale_factor);
  auto monitor_info = MonitorInfoFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  gfx::Rect screen_pixel_bounds(monitor_info->rcMonitor);
  auto screen_bounds =
      gfx::ScaleToEnclosingRect(screen_pixel_bounds, 1.0f / scale_factor);

  const gfx::Point origin = gfx::ToRoundedPoint(ScalePointRelative(
      gfx::PointF(pixel_bounds.origin()), screen_pixel_bounds.origin(),
      screen_bounds.origin(), 1.0f / scale_factor));
  dip_rect.set_origin(origin);
  return dip_rect;
}

gfx::Rect DIPToScreenRect(HWND hwnd, const gfx::Rect& pixel_bounds) {
  float scale_factor = GetScalingFactorFromDPI(GetDPIForHWND(hwnd));
  gfx::Rect screen_rect = ScaleToRoundedRect(pixel_bounds, scale_factor);
  auto monitor_info = MonitorInfoFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  gfx::Rect screen_pixel_bounds(monitor_info->rcMonitor);
  auto screen_bounds =
      gfx::ScaleToEnclosingRect(screen_pixel_bounds, 1.0f / scale_factor);
  const gfx::Point origin = gfx::ToRoundedPoint(ScalePointRelative(
      gfx::PointF(pixel_bounds.origin()), screen_pixel_bounds.origin(),
      screen_bounds.origin(), scale_factor));
  screen_rect.set_origin(origin);
  return screen_rect;
}

float GetScaleFactorForHWND(HWND hwnd) {
  return GetScalingFactorFromDPI(GetDPIForHWND(hwnd));
}

gfx::Size DIPToScreenSize(HWND hwnd, const gfx::Size& dip_size) {
  // Always ceil sizes. Otherwise we may be leaving off part of the bounds.
  return ScaleToCeiledSize(dip_size, GetScaleFactorForHWND(hwnd));
}

gfx::Size ScreenToDIPSize(HWND hwnd, const gfx::Size& size_in_pixels) {
  // Always ceil sizes. Otherwise we may be leaving off part of the bounds.
  return ScaleToCeiledSize(size_in_pixels, 1.0f / GetScaleFactorForHWND(hwnd));
}

}  // namespace lynxtron
