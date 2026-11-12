// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include <X11/extensions/scrnsaver.h>
#include <gdk/x11/gdkx.h>
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
    auto* monitors = gdk_display_get_monitors(gdk_display_get_default());
    g_signal_connect(
        monitors, "items-changed",
        G_CALLBACK(+[](GListModel*, guint, guint, guint, gpointer data) {
          static_cast<LinuxScreen*>(data)->ScheduleRefresh();
        }),
        this);
  }

  gfx::Point GetCursorScreenPoint() override {
    if (!attached_) {
      return {};
    }
    auto* display = gdk_display_get_default();
    if (!GDK_IS_X11_DISPLAY(display)) {
      return {};
    }
    auto* xdisplay = gdk_x11_display_get_xdisplay(display);
    Window root, child;
    int x, y, local_x, local_y;
    unsigned int mask;
    if (!XQueryPointer(xdisplay, DefaultRootWindow(xdisplay), &root, &child, &x,
                       &y, &local_x, &local_y, &mask)) {
      return {};
    }
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
    auto* surface = gtk_native_get_surface(GTK_NATIVE(window));
    auto* monitor = surface ? gdk_display_get_monitor_at_surface(
                                  gdk_display_get_default(), surface)
                            : nullptr;
    for (const auto& display : displays_) {
      if (display.id() == reinterpret_cast<intptr_t>(monitor)) {
        return display;
      }
    }
    return GetPrimaryDisplay();
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
  void ScheduleRefresh() {
    if (refresh_source_) {
      return;
    }
    // A Wayland output update changes geometry and scale in separate GObject
    // notifications. Publish their completed state once the dispatch finishes.
    refresh_source_ = g_idle_add_full(
        G_PRIORITY_DEFAULT_IDLE,
        +[](gpointer data) -> gboolean {
          auto* self = static_cast<LinuxScreen*>(data);
          self->refresh_source_ = 0;
          self->Refresh();
          return G_SOURCE_REMOVE;
        },
        this, nullptr);
  }
  void Refresh() {
    auto old = displays_;
    for (auto* monitor : monitors_) {
      g_signal_handlers_disconnect_by_data(monitor, this);
      g_object_unref(monitor);
    }
    monitors_.clear();
    displays_.clear();
    primary_ = 0;
    auto* screen = gdk_display_get_default();
    auto* model = gdk_display_get_monitors(screen);
    for (guint i = 0; i < g_list_model_get_n_items(model); ++i) {
      auto* monitor = GDK_MONITOR(g_list_model_get_item(model, i));
      monitors_.push_back(monitor);
      g_signal_connect(monitor, "notify",
                       G_CALLBACK(+[](GObject*, GParamSpec*, gpointer data) {
                         static_cast<LinuxScreen*>(data)->ScheduleRefresh();
                       }),
                       this);
      GdkRectangle rect, work;
      gdk_monitor_get_geometry(monitor, &rect);
      // Wayland announces an output before its first geometry arrives. Keep
      // observing it, but expose it only after it has a usable logical extent.
      if (!gdk_monitor_is_valid(monitor) || rect.width <= 0 ||
          rect.height <= 0) {
        continue;
      }
      // Wayland exposes no portable work-area/primary-monitor interface.
      // X11 can obtain its work area; otherwise report monitor bounds.
      work = rect;
      if (GDK_IS_X11_MONITOR(monitor)) {
        gdk_x11_monitor_get_workarea(monitor, &work);
      }
      display::Display display(
          reinterpret_cast<intptr_t>(monitor),
          gfx::Rect(rect.x, rect.y, rect.width, rect.height));
      display.SetScale(gdk_monitor_get_scale(monitor));
      display.set_work_area(gfx::Rect(work.x, work.y, work.width, work.height));
      displays_.push_back(display);
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
      } else {
        uint32_t metrics = 0;
        if (it->bounds() != display.bounds()) {
          metrics |= display::DisplayObserver::DISPLAY_METRIC_BOUNDS;
        }
        if (it->work_area() != display.work_area()) {
          metrics |= display::DisplayObserver::DISPLAY_METRIC_WORK_AREA;
        }
        if (it->device_scale_factor() != display.device_scale_factor()) {
          metrics |=
              display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
        }
        if (metrics) {
          observers_.Notify(&display::DisplayObserver::OnDisplayMetricsChanged,
                            display, metrics);
        }
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
  std::vector<GdkMonitor*> monitors_;
  std::vector<display::Display> displays_;
  base::ObserverList<display::DisplayObserver> observers_;
  size_t primary_ = 0;
  bool attached_ = false;
  guint refresh_source_ = 0;
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
