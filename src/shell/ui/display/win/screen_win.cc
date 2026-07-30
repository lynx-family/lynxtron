// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/ui/display/win/screen_win.h"

#include <windows.h>

#include <shellscalingapi.h>

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_hdc.h"
#include "base/win/win_util.h"
#include "shell/common/win_util.h"
#include "shell/ui/display/display.h"
#include "shell/ui/display/display_layout.h"
#include "shell/ui/display/display_layout_builder.h"
#include "shell/ui/display/win/display_info.h"
#include "shell/ui/display/win/scaling_util.h"
#include "shell/ui/display/win/screen_win_display.h"
#include "shell/ui/gfx/geometry/point_conversions.h"
#include "shell/ui/gfx/geometry/rect.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace display {
namespace win {
namespace {

ScreenWin* g_instance = nullptr;
constexpr float kDefaultDPI = 96.0f;

int GetDefaultSystemDPI() {
  static const int dpi = [] {
    base::win::ScopedGetDC screen_dc(nullptr);
    const int dpi_x = ::GetDeviceCaps(screen_dc, LOGPIXELSX);
    const int dpi_y = ::GetDeviceCaps(screen_dc, LOGPIXELSY);
    DCHECK_EQ(dpi_x, dpi_y);
    return dpi_x;
  }();
  return dpi;
}

int GetDPIFromScaleFactor(float scale_factor) {
  return static_cast<int>(kDefaultDPI * scale_factor);
}

// Gets the DPI for a particular monitor.
absl::optional<int> GetPerMonitorDPI(HMONITOR monitor) {
  if (!lynxtron::IsProcessPerMonitorDpiAware()) {
    return absl::nullopt;
  }

  static auto get_dpi_for_monitor_func = []() {
    const HMODULE shcore_dll = ::LoadLibrary(L"shcore.dll");
    return reinterpret_cast<decltype(&::GetDpiForMonitor)>(
        shcore_dll ? ::GetProcAddress(shcore_dll, "GetDpiForMonitor")
                   : nullptr);
  }();
  UINT dpi_x, dpi_y;
  if (!get_dpi_for_monitor_func ||
      !SUCCEEDED(get_dpi_for_monitor_func(monitor, MDT_EFFECTIVE_DPI, &dpi_x,
                                          &dpi_y))) {
    return absl::nullopt;
  }

  DCHECK_EQ(dpi_x, dpi_y);
  return static_cast<int>(dpi_x);
}

float ScaleFactorFromDPI(int dpi) {
  return static_cast<float>(dpi) / kDefaultDPI;
}

// Gets the raw monitor scale factor.
//
// Respects the forced device scale factor, and will fall back to the global
// scale factor if per-monitor DPI is not supported.
float GetMonitorScaleFactor(HMONITOR monitor) {
  DCHECK(monitor);
  const auto dpi = GetPerMonitorDPI(monitor);
  return ScaleFactorFromDPI(dpi.value_or(GetDefaultSystemDPI()));
}

Display::Rotation OrientationToRotation(DWORD orientation) {
  switch (orientation) {
    case DMDO_DEFAULT:
      return Display::ROTATE_0;
    case DMDO_90:
      return Display::ROTATE_90;
    case DMDO_180:
      return Display::ROTATE_180;
    case DMDO_270:
      return Display::ROTATE_270;
    default:
      NOTREACHED();
      return Display::ROTATE_0;
  }
}

struct DisplaySettings {
  Display::Rotation rotation;
  int frequency;
};
DisplaySettings GetDisplaySettingsForDevice(const wchar_t* device_name) {
  DEVMODE mode = {};
  mode.dmSize = sizeof(mode);
  if (!::EnumDisplaySettings(device_name, ENUM_CURRENT_SETTINGS, &mode)) {
    return {Display::ROTATE_0, 0};
  }
  return {OrientationToRotation(mode.dmDisplayOrientation),
          static_cast<int>(mode.dmDisplayFrequency)};
}

std::vector<DisplayInfo> FindAndRemoveTouchingDisplayInfos(
    const DisplayInfo& parent_info,
    std::vector<DisplayInfo>* display_infos) {
  const auto first_touching_it = std::partition(
      display_infos->begin(), display_infos->end(),
      [&](const auto& info) { return !DisplayInfosTouch(parent_info, info); });
  std::vector<DisplayInfo> touching_display_infos(first_touching_it,
                                                  display_infos->end());
  display_infos->erase(first_touching_it, display_infos->end());
  return touching_display_infos;
}

Display CreateDisplayFromDisplayInfo(const DisplayInfo& display_info) {
  const float scale_factor = display_info.device_scale_factor();
  const gfx::Rect bounds = gfx::ScaleToEnclosingRect(display_info.screen_rect(),
                                                     1.0f / scale_factor);
  Display display(display_info.id(), bounds);
  display.set_device_scale_factor(scale_factor);
  display.set_work_area(gfx::ScaleToEnclosingRect(
      display_info.screen_work_rect(), 1.0f / scale_factor));
  display.set_rotation(display_info.rotation());
  display.set_display_frequency(display_info.display_frequency());
  return display;
}

// Windows historically has had a hard time handling displays of DPIs higher
// than 96. Handling multiple DPI displays means we have to deal with Windows'
// monitor physical coordinates and map into Chrome's DIP coordinates.
//
// To do this, DisplayInfosToScreenWinDisplays reasons over monitors as a tree
// using the primary monitor as the root. All monitors touching this root are
// considered children.
//
// This also presumes that all monitors are connected components. By UI
// construction, Windows restricts the layout of monitors to connected
// components except when DPI virtualization is happening. When this happens, we
// scale relative to (0, 0).
//
// Note that this does not handle cases where a scaled display may have
// insufficient room to lay out its children. In these cases, a DIP point could
// map to multiple screen points due to overlap. The first discovered screen
// will take precedence.
std::vector<ScreenWinDisplay> DisplayInfosToScreenWinDisplays(
    const std::vector<DisplayInfo>& display_infos) {
  if (display_infos.empty()) {
    return {};
  }
  // Find and extract the primary display.
  std::vector<DisplayInfo> display_infos_remaining = display_infos;
  auto primary_display_iter = std::find_if(
      display_infos_remaining.begin(), display_infos_remaining.end(),
      [](const DisplayInfo& display_info) {
        return display_info.screen_rect().origin().IsOrigin();
      });
  DCHECK(primary_display_iter != display_infos_remaining.end());

  // Build the tree and determine DisplayPlacements along the way.
  DisplayLayoutBuilder builder(primary_display_iter->id());
  std::vector<DisplayInfo> available_parents = {*primary_display_iter};
  display_infos_remaining.erase(primary_display_iter);
  while (!available_parents.empty()) {
    const DisplayInfo parent = available_parents.back();
    available_parents.pop_back();
    for (const auto& child :
         FindAndRemoveTouchingDisplayInfos(parent, &display_infos_remaining)) {
      builder.AddDisplayPlacement(CalculateDisplayPlacement(parent, child));
      available_parents.push_back(child);
    }
  }

  // Layout and create the ScreenWinDisplays.
  std::vector<Display> displays;
  for (const auto& display_info : display_infos) {
    displays.push_back(CreateDisplayFromDisplayInfo(display_info));
  }
  builder.Build()->ApplyToDisplayList(&displays, nullptr, 0);

  std::vector<ScreenWinDisplay> screen_win_displays;
  for (size_t i = 0; i < display_infos.size(); ++i) {
    screen_win_displays.emplace_back(displays[i], display_infos[i]);
  }
  return screen_win_displays;
}

std::vector<Display> ScreenWinDisplaysToDisplays(
    const std::vector<ScreenWinDisplay>& screen_win_displays) {
  std::vector<Display> displays;
  for (const auto& screen_win_display : screen_win_displays) {
    displays.push_back(screen_win_display.display());
  }
  return displays;
}

MONITORINFOEX MonitorInfoFromHMONITOR(HMONITOR monitor) {
  MONITORINFOEX monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  ::GetMonitorInfo(monitor, &monitor_info);
  return monitor_info;
}

BOOL CALLBACK EnumMonitorForDisplayInfoCallback(HMONITOR monitor,
                                                HDC hdc,
                                                LPRECT rect,
                                                LPARAM data) {
  const MONITORINFOEX monitor_info = MonitorInfoFromHMONITOR(monitor);
  const auto display_settings =
      GetDisplaySettingsForDevice(monitor_info.szDevice);

  auto* display_infos = reinterpret_cast<std::vector<DisplayInfo>*>(data);
  DCHECK(display_infos);
  display_infos->emplace_back(monitor_info, GetMonitorScaleFactor(monitor),
                              display_settings.rotation,
                              display_settings.frequency);
  return TRUE;
}

std::vector<DisplayInfo> GetDisplayInfosFromSystem() {
  std::vector<DisplayInfo> display_infos;
  EnumDisplayMonitors(nullptr, nullptr, EnumMonitorForDisplayInfoCallback,
                      reinterpret_cast<LPARAM>(&display_infos));
  DCHECK_EQ(::GetSystemMetrics(SM_CMONITORS),
            static_cast<int>(display_infos.size()));
  return display_infos;
}

// Returns |point|, transformed from |from_origin|'s to |to_origin|'s
// coordinates, which differ by |scale_factor|.
gfx::PointF ScalePointRelative(const gfx::PointF& point,
                               const gfx::Point& from_origin,
                               const gfx::Point& to_origin,
                               const float scale_factor) {
  const gfx::PointF relative_point = point - from_origin.OffsetFromOrigin();
  const gfx::PointF scaled_relative_point =
      gfx::ScalePoint(relative_point, scale_factor);
  return scaled_relative_point + to_origin.OffsetFromOrigin();
}

gfx::PointF ScreenToDIPPoint(const gfx::PointF& screen_point,
                             const ScreenWinDisplay& screen_win_display) {
  const Display display = screen_win_display.display();
  return ScalePointRelative(
      screen_point, screen_win_display.pixel_bounds().origin(),
      display.bounds().origin(), 1.0f / display.device_scale_factor());
}

gfx::Point DIPToScreenPoint(const gfx::Point& dip_point,
                            const ScreenWinDisplay& screen_win_display) {
  const Display display = screen_win_display.display();
  // Rounding in both directions preserves integer DIP coordinates across a
  // DIP-to-screen-to-DIP round trip and avoids systematic layout drift.
  return gfx::ToRoundedPoint(
      ScalePointRelative(gfx::PointF(dip_point), display.bounds().origin(),
                         screen_win_display.pixel_bounds().origin(),
                         display.device_scale_factor()));
}

}  // namespace

ScreenWin::ScreenWin() : ScreenWin(true) {}

ScreenWin::ScreenWin(bool initialize) {
  DCHECK(!g_instance);
  g_instance = this;
  if (initialize) {
    Initialize();
  }
}

ScreenWin::~ScreenWin() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
gfx::PointF ScreenWin::ScreenToDIPPoint(const gfx::PointF& pixel_point) {
  const ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestScreenPoint,
                             gfx::ToFlooredPoint(pixel_point));
  return display::win::ScreenToDIPPoint(pixel_point, screen_win_display);
}

// static
gfx::Point ScreenWin::DIPToScreenPoint(const gfx::Point& dip_point) {
  const ScreenWinDisplay screen_win_display = GetScreenWinDisplayVia(
      &ScreenWin::GetScreenWinDisplayNearestDIPPoint, dip_point);
  return display::win::DIPToScreenPoint(dip_point, screen_win_display);
}

// static
gfx::Rect ScreenWin::ScreenToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds) {
  const ScreenWinDisplay screen_win_display =
      hwnd
          ? GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestHWND,
                                   hwnd)
          : GetScreenWinDisplayVia(
                &ScreenWin::GetScreenWinDisplayNearestScreenRect, pixel_bounds);
  const gfx::Point origin = gfx::ToRoundedPoint(display::win::ScreenToDIPPoint(
      gfx::PointF(pixel_bounds.origin()), screen_win_display));
  const float scale_factor =
      1.0f / screen_win_display.display().device_scale_factor();
  // ScreenWin keeps Chromium's generic enclosing-size semantics. Window API
  // callers replace this size with their rounded policy in dpi_win.cc.
  return {origin, ScaleToEnclosingRect(pixel_bounds, scale_factor).size()};
}

// static
gfx::Rect ScreenWin::DIPToScreenRect(HWND hwnd, const gfx::Rect& dip_bounds) {
  // The HWND parameter is needed for cases where Chrome windows span monitors
  // that have different DPI settings. This is known to matter when using the OS
  // IME support. See https::/crbug.com/1224715 for more details.
  const ScreenWinDisplay screen_win_display =
      hwnd ? GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestHWND,
                                    hwnd)
           : GetScreenWinDisplayVia(
                 &ScreenWin::GetScreenWinDisplayNearestDIPRect, dip_bounds);
  const gfx::Point origin =
      display::win::DIPToScreenPoint(dip_bounds.origin(), screen_win_display);
  const float scale_factor = screen_win_display.display().device_scale_factor();
  // ScreenWin keeps Chromium's generic enclosing-size semantics. Window API
  // callers replace this size with their rounded policy in dpi_win.cc.
  return {origin, ScaleToEnclosingRect(dip_bounds, scale_factor).size()};
}

// static
int ScreenWin::GetSystemMetricsForMonitor(HMONITOR monitor, int metric) {
  if (!g_instance) {
    return ::GetSystemMetrics(metric);
  }

  // Fall back to the primary display's HMONITOR.
  if (!monitor) {
    monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
  }

  // We'll then pull up the system metrics scaled by the appropriate amount.
  return g_instance->GetSystemMetricsForScaleFactor(
      GetMonitorScaleFactor(monitor), metric);
}

// static
float ScreenWin::GetScaleFactorForHWND(HWND hwnd) {
  const HWND root_hwnd = g_instance ? g_instance->GetRootWindow(hwnd) : hwnd;
  const ScreenWinDisplay screen_win_display = GetScreenWinDisplayVia(
      &ScreenWin::GetScreenWinDisplayNearestHWND, root_hwnd);
  return screen_win_display.display().device_scale_factor();
}

// static
float ScreenWin::GetScaleFactorForScreenRect(const gfx::Rect& pixel_bounds) {
  return GetScreenWinDisplayVia(
             &ScreenWin::GetScreenWinDisplayNearestScreenRect, pixel_bounds)
      .display()
      .device_scale_factor();
}

// static
int ScreenWin::GetDPIForScreenRect(const gfx::Rect& pixel_bounds) {
  const RECT rect = pixel_bounds.ToRECT();
  const HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
  return GetPerMonitorDPI(monitor).value_or(GetDefaultSystemDPI());
}

// static
float ScreenWin::GetScaleFactorForDIPRect(const gfx::Rect& dip_bounds) {
  return GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestDIPRect,
                                dip_bounds)
      .display()
      .device_scale_factor();
}

// static
int ScreenWin::GetDPIForHWND(HWND hwnd) {
  const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  return GetPerMonitorDPI(monitor).value_or(GetDefaultSystemDPI());
}

// static
float ScreenWin::GetScaleFactorForDPI(int dpi) {
  return ScaleFactorFromDPI(dpi);
}

gfx::Point ScreenWin::GetCursorScreenPoint() {
  POINT pt;
  ::GetCursorPos(&pt);
  return gfx::ToFlooredPoint(ScreenToDIPPoint(gfx::PointF(gfx::Point(pt))));
}

int ScreenWin::GetNumDisplays() const {
  return static_cast<int>(screen_win_displays_.size());
}

const std::vector<Display>& ScreenWin::GetAllDisplays() const {
  return displays_;
}

Display ScreenWin::GetDisplayNearestWindow(gfx::NativeWindow window) const {
  // When |window| isn't rooted to a display, we should just return the default
  // display so we get some correct display information like the scaling factor.
  return window ? GetScreenWinDisplayNearestHWND(window).display()
                : GetPrimaryDisplay();
}

Display ScreenWin::GetDisplayNearestPoint(const gfx::Point& point) const {
  const gfx::Point screen_point = DIPToScreenPoint(point);
  return GetScreenWinDisplayNearestScreenPoint(screen_point).display();
}

Display ScreenWin::GetDisplayMatching(const gfx::Rect& match_rect) const {
  const gfx::Rect screen_rect = DIPToScreenRect(nullptr, match_rect);
  return GetScreenWinDisplayNearestScreenRect(screen_rect).display();
}

Display ScreenWin::GetPrimaryDisplay() const {
  return GetPrimaryScreenWinDisplay().display();
}

void ScreenWin::AddObserver(DisplayObserver* observer) {
  change_notifier_.AddObserver(observer);
}

void ScreenWin::RemoveObserver(DisplayObserver* observer) {
  change_notifier_.RemoveObserver(observer);
}

void ScreenWin::UpdateFromDisplayInfos(
    const std::vector<DisplayInfo>& display_infos) {
  screen_win_displays_ = DisplayInfosToScreenWinDisplays(display_infos);
  displays_ = ScreenWinDisplaysToDisplays(screen_win_displays_);
}

void ScreenWin::Initialize() {
  singleton_hwnd_observer_ = std::make_unique<gfx::SingletonHwndObserver>(
      base::BindRepeating(&ScreenWin::OnWndProc, base::Unretained(this)));
  UpdateFromDisplayInfos(GetDisplayInfosFromSystem());

  // Ping Windows.UI.dll from being unloaded by clay.
  HMODULE winuidllHandle{};
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"Windows.UI.dll",
                     &winuidllHandle);
  RecordDisplayScaleFactors();
}

MONITORINFOEX ScreenWin::MonitorInfoFromScreenPoint(
    const gfx::Point& screen_point) const {
  return MonitorInfoFromHMONITOR(
      ::MonitorFromPoint(screen_point.ToPOINT(), MONITOR_DEFAULTTONEAREST));
}

MONITORINFOEX ScreenWin::MonitorInfoFromScreenRect(
    const gfx::Rect& screen_rect) const {
  const RECT win_rect = screen_rect.ToRECT();
  return MonitorInfoFromHMONITOR(
      ::MonitorFromRect(&win_rect, MONITOR_DEFAULTTONEAREST));
}

MONITORINFOEX ScreenWin::MonitorInfoFromWindow(HWND hwnd,
                                               DWORD default_options) const {
  return MonitorInfoFromHMONITOR(::MonitorFromWindow(hwnd, default_options));
}

HWND ScreenWin::GetRootWindow(HWND hwnd) const {
  return ::GetAncestor(hwnd, GA_ROOT);
}

int ScreenWin::GetSystemMetrics(int metric) const {
  return ::GetSystemMetrics(metric);
}

void ScreenWin::OnWndProc(HWND, UINT message, WPARAM wparam, LPARAM) {
  // Refresh not only for explicit display/work-area changes but also when the
  // app becomes active. Windows may coalesce or omit notifications while the
  // process is inactive, so activation is the synchronization fallback used by
  // Chromium's ScreenWin behavior.
  if (message != WM_DISPLAYCHANGE &&
      (message != WM_ACTIVATEAPP || wparam != TRUE) &&
      (message != WM_SETTINGCHANGE || wparam != SPI_SETWORKAREA)) {
    return;
  }

  TRACE_EVENT1("ui", "ScreenWin::OnWndProc", "message", message);

  UpdateAllDisplaysAndNotify();
}

void ScreenWin::UpdateAllDisplaysAndNotify() {
  TRACE_EVENT0("ui", "ScreenWin::UpdateAllDisplaysAndNotify");

  std::vector<Display> old_displays = std::move(displays_);
  UpdateFromDisplayInfos(GetDisplayInfosFromSystem());
  change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestHWND(HWND hwnd) const {
  return GetScreenWinDisplay(
      MonitorInfoFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestScreenRect(
    const gfx::Rect& screen_rect) const {
  return GetScreenWinDisplay(MonitorInfoFromScreenRect(screen_rect));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestScreenPoint(
    const gfx::Point& screen_point) const {
  return GetScreenWinDisplay(MonitorInfoFromScreenPoint(screen_point));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestDIPPoint(
    const gfx::Point& dip_point) const {
  ScreenWinDisplay primary_screen_win_display;
  for (const auto& screen_win_display : screen_win_displays_) {
    const gfx::Rect dip_bounds = screen_win_display.display().bounds();
    if (dip_bounds.Contains(dip_point)) {
      return screen_win_display;
    }
    if (dip_bounds.origin().IsOrigin()) {
      primary_screen_win_display = screen_win_display;
    }
  }
  return primary_screen_win_display;
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestDIPRect(
    const gfx::Rect& dip_rect) const {
  const auto first_closer = [dip_rect](const auto& display1,
                                       const auto& display2) {
    return SquaredDistanceBetweenRects(dip_rect, display1.display().bounds()) <
           SquaredDistanceBetweenRects(dip_rect, display2.display().bounds());
  };
  const auto it = std::min_element(screen_win_displays_.cbegin(),
                                   screen_win_displays_.cend(), first_closer);
  return (it == screen_win_displays_.cend()) ? ScreenWinDisplay() : *it;
}

ScreenWinDisplay ScreenWin::GetPrimaryScreenWinDisplay() const {
  const ScreenWinDisplay screen_win_display = GetScreenWinDisplay(
      MonitorInfoFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY));
  // The Windows primary monitor is defined to have an origin of (0, 0).
  DCHECK(screen_win_display.display().bounds().origin().IsOrigin());
  return screen_win_display;
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplay(
    const MONITORINFOEX& monitor_info) const {
  const int64_t id = DisplayInfo::DeviceIdFromDeviceName(monitor_info.szDevice);
  const auto it = std::find_if(
      screen_win_displays_.cbegin(), screen_win_displays_.cend(),
      [id](const auto& display) { return display.display().id() == id; });
  // There is 1:1 correspondence between MONITORINFOEX and ScreenWinDisplay.
  // If we found no screens, either there are no screens, or we're in the midst
  // of updating our screens (see crbug.com/768845); either way, hand out the
  // default display.
  return (it == screen_win_displays_.cend()) ? ScreenWinDisplay() : *it;
}

// static
template <typename Getter, typename GetterType>
ScreenWinDisplay ScreenWin::GetScreenWinDisplayVia(Getter getter,
                                                   GetterType value) {
  return g_instance ? (g_instance->*getter)(value) : ScreenWinDisplay();
}

int ScreenWin::GetSystemMetricsForScaleFactor(float scale_factor,
                                              int metric) const {
  if (lynxtron::IsProcessPerMonitorDpiAware()) {
    static const auto get_system_metrics_for_dpi =
        reinterpret_cast<decltype(&::GetSystemMetricsForDpi)>(
            base::win::GetUser32FunctionPointer("GetSystemMetricsForDpi"));
    if (get_system_metrics_for_dpi) {
      return get_system_metrics_for_dpi(metric,
                                        GetDPIFromScaleFactor(scale_factor));
    }
  }

  // Windows 8.1 doesn't support GetSystemMetricsForDpi(), yet does support
  // per-process dpi awareness.
  return base::ClampRound(GetSystemMetrics(metric) * scale_factor /
                          GetPrimaryDisplay().device_scale_factor());
}

void ScreenWin::RecordDisplayScaleFactors() const {
  std::vector<int> unique_scale_factors;
  for (const auto& screen_win_display : screen_win_displays_) {
    const float scale_factor =
        screen_win_display.display().device_scale_factor();
    // Multiply the reported value by 100 to display it as a percentage. Clamp
    // it so that if it's wildly out-of-band we won't send it to the backend.
    const int reported_scale =
        std::clamp(base::checked_cast<int>(scale_factor * 100), 0, 1000);
    if (!base::Contains(unique_scale_factors, reported_scale)) {
      unique_scale_factors.push_back(reported_scale);
      base::UmaHistogramSparse("UI.DeviceScale", reported_scale);
    }
  }
}

ScreenWin* GetScreenWin() {
  return g_instance;
}

}  // namespace win
}  // namespace display
