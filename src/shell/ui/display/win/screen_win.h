// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_DISPLAY_WIN_SCREEN_WIN_H_
#define LYNXTRON_SHELL_UI_DISPLAY_WIN_SCREEN_WIN_H_

#include <windows.h>

#include <memory>
#include <vector>

#include "shell/ui/display/display_change_notifier.h"
#include "shell/ui/display/screen.h"
#include "shell/ui/gfx/native_ui_types.h"
#include "shell/ui/gfx/win/singleton_hwnd_observer.h"

namespace gfx {
class Display;
class Point;
class PointF;
class Rect;
}  // namespace gfx

namespace display {
namespace win {

class DisplayInfo;
class ScreenWinDisplay;

class ScreenWin : public Screen {
 public:
  ScreenWin();
  ~ScreenWin() override;

  // Converts a screen physical point to a screen DIP point.
  // The DPI scale is performed relative to the display containing the physical
  // point.
  static gfx::PointF ScreenToDIPPoint(const gfx::PointF& pixel_point);

  // Converts a screen DIP point to a screen physical point.
  // The DPI scale is performed relative to the display containing the DIP
  // point.
  static gfx::Point DIPToScreenPoint(const gfx::Point& dip_point);

  // WARNING: There is no right way to scale sizes and rects.
  // Sometimes you may need the enclosing rect (which favors transformations
  // that stretch the bounds towards integral values) or the enclosed rect
  // (transformations that shrink the bounds towards integral values).
  // This implementation favors the enclosing rect.
  //
  // These are generic display-coordinate helpers. NativeWindow geometry must
  // use shell/api/dpi_win.h instead: window API sizes use independent rounded
  // conversion so their result cannot vary with the rect origin.
  //
  // Understand which you need before blindly assuming this is the right way.

  // Converts a screen physical rect to a screen DIP rect.
  // The DPI scale is performed relative to the display nearest to |hwnd|.
  // If |hwnd| is null, scaling will be performed to the display nearest to
  // |pixel_bounds|.
  static gfx::Rect ScreenToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds);

  // Converts a screen DIP rect to a screen physical rect.
  // If |hwnd| is null, scaling will be performed using the DSF of the display
  // nearest to |dip_bounds|; otherwise, scaling will be performed using the DSF
  // of the display nearest to |hwnd|.  Thus if an existing HWND is moving to a
  // different display, it's often more correct to pass null for |hwnd| to get
  // the new display's scale factor rather than the old one's.
  static gfx::Rect DIPToScreenRect(HWND hwnd, const gfx::Rect& dip_bounds);

  // Returns the result of GetSystemMetrics for |metric| scaled to |monitor|'s
  // DPI. Use this function if you're already working with screen pixels, as
  // this helps reduce any cascading rounding errors from DIP to the |monitor|'s
  // DPI.
  //
  // Note that metrics which correspond to elements drawn by Windows
  // (specifically frame and resize handles) will be scaled by DPI only and not
  // by Text Zoom or other accessibility features.
  static int GetSystemMetricsForMonitor(HMONITOR monitor, int metric);

  // Returns |hwnd|'s scale factor, including accessibility adjustments.
  static float GetScaleFactorForHWND(HWND hwnd);

  // Returns the scale factor selected by the same display-selection rules as
  // ScreenToDIPRect(nullptr, |pixel_bounds|). This lets window-specific
  // wrappers reuse ScreenWin's monitor choice while applying a different size
  // policy.
  static float GetScaleFactorForScreenRect(const gfx::Rect& pixel_bounds);

  // Returns the unmodified DPI for the display nearest to |pixel_bounds|.
  static int GetDPIForScreenRect(const gfx::Rect& pixel_bounds);

  // Returns the scale factor selected by the same display-selection rules as
  // DIPToScreenRect(nullptr, |dip_bounds|). This lets window-specific wrappers
  // reuse ScreenWin's monitor choice while applying a different size policy.
  static float GetScaleFactorForDIPRect(const gfx::Rect& dip_bounds);

  // Returns the unmodified DPI for a particular |hwnd|, without accessibility
  // adjustments.
  static int GetDPIForHWND(HWND hwnd);

  // Converts dpi to scale factor, including accessibility adjustments.
  static float GetScaleFactorForDPI(int dpi);

 protected:
  // Tests can skip system enumeration and inject deterministic DisplayInfos,
  // for example to exercise fractional scale factors independent of the host.
  explicit ScreenWin(bool initialize);

  // Screen:
  gfx::Point GetCursorScreenPoint() override;
  int GetNumDisplays() const override;
  const std::vector<Display>& GetAllDisplays() const override;
  Display GetDisplayNearestWindow(gfx::NativeWindow window) const override;
  Display GetDisplayNearestPoint(const gfx::Point& point) const override;
  Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  Display GetPrimaryDisplay() const override;
  void AddObserver(DisplayObserver* observer) override;
  void RemoveObserver(DisplayObserver* observer) override;

  void UpdateFromDisplayInfos(const std::vector<DisplayInfo>& display_infos);

  MONITORINFOEX MonitorInfoFromScreenPoint(
      const gfx::Point& screen_point) const;
  MONITORINFOEX MonitorInfoFromScreenRect(const gfx::Rect& screen_rect) const;
  MONITORINFOEX MonitorInfoFromWindow(HWND hwnd, DWORD default_options) const;
  HWND GetRootWindow(HWND hwnd) const;
  int GetSystemMetrics(int metric) const;

 private:
  void Initialize();
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  void UpdateAllDisplaysAndNotify();

  // Returns the ScreenWinDisplay closest to or enclosing |hwnd|.
  ScreenWinDisplay GetScreenWinDisplayNearestHWND(HWND hwnd) const;

  // Returns the ScreenWinDisplay closest to or enclosing |screen_rect|.
  ScreenWinDisplay GetScreenWinDisplayNearestScreenRect(
      const gfx::Rect& screen_rect) const;

  // Returns the ScreenWinDisplay closest to or enclosing |screen_point|.
  ScreenWinDisplay GetScreenWinDisplayNearestScreenPoint(
      const gfx::Point& screen_point) const;

  // Returns the ScreenWinDisplay closest to or enclosing |dip_point|.
  ScreenWinDisplay GetScreenWinDisplayNearestDIPPoint(
      const gfx::Point& dip_point) const;

  // Returns the ScreenWinDisplay closest to or enclosing |dip_rect|.
  ScreenWinDisplay GetScreenWinDisplayNearestDIPRect(
      const gfx::Rect& dip_rect) const;

  // Returns the ScreenWinDisplay corresponding to the primary monitor.
  ScreenWinDisplay GetPrimaryScreenWinDisplay() const;

  ScreenWinDisplay GetScreenWinDisplay(const MONITORINFOEX& monitor_info) const;

  // Returns the result of calling |getter| with |value| on the global
  // ScreenWin if it exists, otherwise return the default ScreenWinDisplay.
  template <typename Getter, typename GetterType>
  static ScreenWinDisplay GetScreenWinDisplayVia(Getter getter,
                                                 GetterType value);

  // Returns the result of GetSystemMetrics for |metric| scaled to the specified
  // |scale_factor|.
  int GetSystemMetricsForScaleFactor(float scale_factor, int metric) const;

  void RecordDisplayScaleFactors() const;

  // Helper implementing the DisplayObserver handling.
  DisplayChangeNotifier change_notifier_;

  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;

  // Current list of ScreenWinDisplays.
  std::vector<ScreenWinDisplay> screen_win_displays_;

  // The Displays corresponding to |screen_win_displays_| for GetAllDisplays().
  // This must be updated anytime |screen_win_displays_| is updated.
  std::vector<Display> displays_;
};

ScreenWin* GetScreenWin();

}  // namespace win
}  // namespace display

#endif  // LYNXTRON_SHELL_UI_DISPLAY_WIN_SCREEN_WIN_H_
