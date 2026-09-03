// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/ui/display/desktop_screen.h"
#include "ui/display/screen.h"

#include <window_manager/oh_display_manager.h>

#include <vector>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace views {

namespace {

display::Display MakeDisplayFromOHOS(
    const NativeDisplayManager_DisplayInfo* info) {
  float scale = info->densityPixels > 0 ? info->densityPixels : 1.0f;
  display::Display d(static_cast<int64_t>(info->id));
  d.set_bounds(gfx::Rect(0, 0,
                         static_cast<int>(info->width / scale),
                         static_cast<int>(info->height / scale)));
  // Use OH_NativeDisplayManager_CreateAvailableArea (since API 20) to get the
  // real available area with its origin offset (status bar, cutout, etc).
  NativeDisplayManager_Rect* area = nullptr;
  if (OH_NativeDisplayManager_CreateAvailableArea(info->id, &area) ==
          DISPLAY_MANAGER_OK &&
      area) {
    d.set_work_area(gfx::Rect(static_cast<int>(area->left / scale),
                              static_cast<int>(area->top / scale),
                              static_cast<int>(area->width / scale),
                              static_cast<int>(area->height / scale)));
    OH_NativeDisplayManager_DestroyAvailableArea(area);
  } else {
    // Fallback: keep the legacy full-area behavior.
    d.set_work_area(gfx::Rect(0, 0,
                              static_cast<int>(info->availableWidth / scale),
                              static_cast<int>(info->availableHeight / scale)));
  }
  d.set_device_scale_factor(scale);
  d.set_display_frequency(static_cast<float>(info->refreshRate));
  d.set_rotation(static_cast<display::Display::Rotation>(info->rotation));
  return d;
}

// Determine whether a display is the internal panel by its source mode
// (DISPLAY_SOURCE_MODE_MAIN == built-in main screen, since API 20).
bool IsInternalDisplay(uint64_t display_id) {
  NativeDisplayManager_SourceMode mode;
  if (OH_NativeDisplayManager_GetDisplaySourceMode(display_id, &mode) !=
      DISPLAY_MANAGER_OK) {
    return false;
  }
  return mode == DISPLAY_SOURCE_MODE_MAIN;
}

}  // namespace

class DesktopScreenHarmony;

static DesktopScreenHarmony* g_instance = nullptr;

class DesktopScreenHarmony : public display::Screen {
 public:
  DesktopScreenHarmony() {
    display::Screen::SetScreenInstance(this);
    g_instance = this;
    ui_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

    LOG(INFO) << "[LynxtronScreen] DesktopScreenHarmony created";

    uint32_t listener_index = 0;
    NativeDisplayManager_ErrorCode ret =
        OH_NativeDisplayManager_RegisterDisplayChangeListener(
            &OnDisplayChanged, &listener_index);
    if (ret == DISPLAY_MANAGER_OK) {
      display_change_listener_index_ = listener_index;
      display_change_registered_ = true;
      LOG(INFO) << "[LynxtronScreen] display change listener registered";
    } else {
      LOG(WARNING) << "[LynxtronScreen] RegisterDisplayChangeListener failed ret="
                   << static_cast<int>(ret);
    }
  }

  ~DesktopScreenHarmony() override {
    display::Screen::SetScreenInstance(nullptr);
    if (display_change_registered_) {
      OH_NativeDisplayManager_UnregisterDisplayChangeListener(
          display_change_listener_index_);
    }
    g_instance = nullptr;
  }

  gfx::Point GetCursorScreenPoint() override {
    return gfx::Point(0, 0);
  }

  int GetNumDisplays() const override {
    if (displays_.empty())
      const_cast<DesktopScreenHarmony*>(this)->FetchDisplays();
    return static_cast<int>(displays_.size());
  }

  const std::vector<display::Display>& GetAllDisplays() const override {
    if (displays_.empty())
      const_cast<DesktopScreenHarmony*>(this)->FetchDisplays();
    return displays_;
  }

  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override {
    return GetPrimaryDisplay();
  }

  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override {
    if (displays_.empty())
      const_cast<DesktopScreenHarmony*>(this)->FetchDisplays();
    for (const auto& d : displays_) {
      if (d.bounds().Contains(point))
        return d;
    }
    return GetPrimaryDisplay();
  }

  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override {
    if (displays_.empty())
      const_cast<DesktopScreenHarmony*>(this)->FetchDisplays();
    const display::Display* best = nullptr;
    int best_area = -1;
    for (const auto& d : displays_) {
      gfx::Rect intersection = gfx::IntersectRects(d.bounds(), match_rect);
      int area = intersection.width() * intersection.height();
      if (area > best_area) {
        best_area = area;
        best = &d;
      }
    }
    return best ? *best : GetPrimaryDisplay();
  }

  display::Display GetPrimaryDisplay() const override {
    NativeDisplayManager_DisplayInfo* info = nullptr;
    NativeDisplayManager_ErrorCode ret =
        OH_NativeDisplayManager_CreatePrimaryDisplay(&info);
    if (ret != DISPLAY_MANAGER_OK || !info) {
      LOG(WARNING) << "[LynxtronScreen] CreatePrimaryDisplay failed ret="
                   << static_cast<int>(ret);
      return display::Display::GetDefaultDisplay();
    }
    // Mark primary as internal only when its source mode is MAIN.
    if (IsInternalDisplay(info->id)) {
      display::SetInternalDisplayIds(
          base::flat_set<int64_t>({static_cast<int64_t>(info->id)}));
    }
    display::Display d = MakeDisplayFromOHOS(info);
    OH_NativeDisplayManager_DestroyDisplay(info);
    return d;
  }

  void AddObserver(display::DisplayObserver* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(display::DisplayObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

 private:
  void FetchDisplays() {
    displays_.clear();
    NativeDisplayManager_DisplaysInfo* info = nullptr;
    NativeDisplayManager_ErrorCode ret =
        OH_NativeDisplayManager_CreateAllDisplays(&info);
    if (ret != DISPLAY_MANAGER_OK || !info) {
      LOG(WARNING) << "[LynxtronScreen] CreateAllDisplays failed ret="
                   << static_cast<int>(ret);
      return;
    }

    uint32_t count = info->displaysLength;
    LOG(INFO) << "[LynxtronScreen] CreateAllDisplays count=" << count;

    displays_.clear();
    displays_.reserve(count);
    base::flat_set<int64_t> internal_ids;
    for (uint32_t i = 0; i < count; ++i) {
      const auto* di = &info->displaysInfo[i];
      displays_.push_back(MakeDisplayFromOHOS(di));
      // Only displays in MAIN source mode are internal.
      if (di->isAlive && IsInternalDisplay(di->id)) {
        internal_ids.insert(static_cast<int64_t>(di->id));
        LOG(INFO) << "[LynxtronScreen] display id=" << di->id
                  << " name=" << di->name << " internal=true";
      }
    }
    if (!internal_ids.empty()) {
      display::SetInternalDisplayIds(std::move(internal_ids));
    }
    OH_NativeDisplayManager_DestroyAllDisplays(info);
  }

  static void OnDisplayChanged(uint64_t displayId) {
    if (g_instance && g_instance->ui_task_runner_) {
      g_instance->ui_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&DesktopScreenHarmony::OnDisplayChangedOnUIThread,
                         base::Unretained(g_instance)));
    }
  }

  void OnDisplayChangedOnUIThread() {
    if (g_instance != this)
      return;
    FetchDisplays();
    display::Display primary = GetPrimaryDisplay();
    for (auto& observer : observers_)
      observer.OnDisplayMetricsChanged(primary, 0);
  }

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  std::vector<display::Display> displays_;
  base::ObserverList<display::DisplayObserver> observers_;
  uint32_t display_change_listener_index_ = 0;
  bool display_change_registered_ = false;
};

std::unique_ptr<display::Screen> CreateDesktopScreen() {
  return std::make_unique<DesktopScreenHarmony>();
}

}  // namespace views
