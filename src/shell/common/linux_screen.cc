// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include <X11/extensions/scrnsaver.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <limits>

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
namespace lynxtron {
namespace {
class LinuxScreen : public display::Screen {
 public:
  LinuxScreen() { displays_.emplace_back(1, gfx::Rect(0, 0, 1280, 800)); }
  void Attach() {
    if (attached_ || !gdk_display_get_default()) {
      return;
    }
    attached_ = true;
    Refresh();
    auto* display = gdk_display_get_default();
    g_signal_connect(display, "monitor-added",
                     G_CALLBACK(+[](GdkDisplay*, GdkMonitor*, gpointer data) {
                       static_cast<LinuxScreen*>(data)->Refresh();
                     }),
                     this);
    g_signal_connect(display, "monitor-removed",
                     G_CALLBACK(+[](GdkDisplay*, GdkMonitor*, gpointer data) {
                       static_cast<LinuxScreen*>(data)->Refresh();
                     }),
                     this);
    auto* screen = gdk_screen_get_default();
    g_signal_connect(screen, "monitors-changed",
                     G_CALLBACK(+[](GdkScreen*, gpointer data) {
                       static_cast<LinuxScreen*>(data)->Refresh();
                     }),
                     this);
  }
  gfx::Point GetCursorScreenPoint() override {
    if (!attached_) {
      return {};
    }
    int x, y;
    gdk_device_get_position(gdk_seat_get_pointer(gdk_display_get_default_seat(
                                gdk_display_get_default())),
                            nullptr, &x, &y);
    return gfx::Point(x, y);
  }
  base::TimeDelta CalculateIdleTime() const override {
    if (!attached_ || !GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
      return base::TimeDelta();
    }
    auto* display = gdk_x11_display_get_xdisplay(gdk_display_get_default());
    auto* info = XScreenSaverAllocInfo();
    bool success =
        XScreenSaverQueryInfo(display, DefaultRootWindow(display), info);
    auto idle = success ? base::Milliseconds(info->idle) : base::TimeDelta();
    XFree(info);
    return idle;
  }
  bool IsScreenSaverActive() const override {
    if (!attached_ || !GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
      return false;
    }
    auto* display = gdk_x11_display_get_xdisplay(gdk_display_get_default());
    auto* info = XScreenSaverAllocInfo();
    bool success =
        XScreenSaverQueryInfo(display, DefaultRootWindow(display), info);
    bool active = success && info->state == ScreenSaverOn;
    XFree(info);
    return active;
  }
  int GetNumDisplays() const override { return displays_.size(); }
  const std::vector<display::Display>& GetAllDisplays() const override {
    return displays_;
  }
  display::Display GetPrimaryDisplay() const override {
    return displays_[primary_];
  }
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override {
    if (!window || !attached_) {
      return GetPrimaryDisplay();
    }
    int x, y;
    gtk_window_get_position(GTK_WINDOW(window), &x, &y);
    return GetDisplayNearestPoint(gfx::Point(x, y));
  }
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override {
    const display::Display* best = &displays_[primary_];
    int64_t distance = std::numeric_limits<int64_t>::max();
    for (const auto& display : displays_) {
      auto rect = display.bounds();
      int64_t dx =
          std::max({rect.x() - point.x(), 0, point.x() - rect.right()});
      int64_t dy =
          std::max({rect.y() - point.y(), 0, point.y() - rect.bottom()});
      if (dx * dx + dy * dy < distance) {
        distance = dx * dx + dy * dy;
        best = &display;
      }
    }
    return *best;
  }
  display::Display GetDisplayMatching(const gfx::Rect& rect) const override {
    const display::Display* best = &displays_[primary_];
    int area = 0;
    for (const auto& display : displays_) {
      auto intersection = display.bounds();
      intersection.Intersect(rect);
      if (intersection.width() * intersection.height() > area) {
        area = intersection.width() * intersection.height();
        best = &display;
      }
    }
    return area ? *best : GetDisplayNearestPoint(rect.CenterPoint());
  }
  bool IsHeadless() const override { return !attached_; }
  void AddObserver(display::DisplayObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(display::DisplayObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

 private:
  void Refresh() {
    auto old = displays_;
    displays_.clear();
    primary_ = 0;
    auto* screen = gdk_display_get_default();
    for (int i = 0; i < gdk_display_get_n_monitors(screen); ++i) {
      auto* monitor = gdk_display_get_monitor(screen, i);
      GdkRectangle rect, work;
      gdk_monitor_get_geometry(monitor, &rect);
      gdk_monitor_get_workarea(monitor, &work);
      display::Display display(
          reinterpret_cast<intptr_t>(monitor),
          gfx::Rect(rect.x, rect.y, rect.width, rect.height));
      display.SetScale(gdk_monitor_get_scale_factor(monitor));
      display.set_work_area(gfx::Rect(work.x, work.y, work.width, work.height));
      displays_.push_back(display);
      if (monitor == gdk_display_get_primary_monitor(screen)) {
        primary_ = i;
      }
    }
    if (displays_.empty()) {
      displays_.emplace_back(1, gfx::Rect(0, 0, 1280, 800));
    }
    for (const auto& display : displays_) {
      auto it = std::find_if(old.begin(), old.end(), [&](const auto& item) {
        return item.id() == display.id();
      });
      if (it == old.end()) {
        observers_.Notify(&display::DisplayObserver::OnDisplayAdded, display);
      } else if (it->bounds() != display.bounds() ||
                 it->work_area() != display.work_area() ||
                 it->device_scale_factor() != display.device_scale_factor()) {
        observers_.Notify(
            &display::DisplayObserver::OnDisplayMetricsChanged, display,
            display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
                display::DisplayObserver::DISPLAY_METRIC_WORK_AREA |
                display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR);
      }
    }
    display::Displays removed;
    for (const auto& display : old) {
      if (std::none_of(
              displays_.begin(), displays_.end(),
              [&](const auto& item) { return item.id() == display.id(); })) {
        removed.push_back(display);
      }
    }
    if (!removed.empty()) {
      observers_.Notify(&display::DisplayObserver::OnDisplaysRemoved, removed);
    }
  }
  std::vector<display::Display> displays_;
  base::ObserverList<display::DisplayObserver> observers_;
  size_t primary_ = 0;
  bool attached_ = false;
};
}  // namespace
void InitializeLinuxScreen() {
  static base::NoDestructor<LinuxScreen> screen;
  if (!display::Screen::Get()) {
    display::Screen::SetScreenInstance(screen.get());
  }
  screen->Attach();
}
}  // namespace lynxtron
