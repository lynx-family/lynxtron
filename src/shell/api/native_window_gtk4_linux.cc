// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gdk/x11/gdkx.h>
#include <gtk/gtk.h>

#include <algorithm>

#include "base/check.h"
#include "base/logging.h"
#include "shell/app/native_window.h"
#include "shell/common/linux_desktop.h"

namespace lynxtron {

namespace {

class NativeWindowLinux : public NativeWindow {
 public:
  NativeWindowLinux(const gin_helper::Dictionary& options, NativeWindow* parent)
      : NativeWindow(options, parent),
        normal_bounds_(0, 0, width(), height()),
        bounds_(0, 0, width(), height()) {
    CHECK(EnsureLinuxDesktop())
        << "Cannot open Linux display; use windowless mode without DISPLAY.";
    widget_ = gtk_window_new();
    auto* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(widget_), content);
    g_object_set_data(G_OBJECT(widget_), "lynxtron-content", content);
    ApplyLinuxApplicationMenu(this);
    gtk_window_set_default_size(GTK_WINDOW(widget_), width(), height());
    gtk_window_set_decorated(GTK_WINDOW(widget_), frame());
    gtk_window_set_resizable(GTK_WINDOW(widget_), resizable());
    gtk_window_set_modal(GTK_WINDOW(widget_), is_modal());
    SetParentWindow(parent);
    g_signal_connect(widget_, "close-request",
                     G_CALLBACK(+[](GtkWindow*, gpointer data) -> gboolean {
                       auto* self = static_cast<NativeWindowLinux*>(data);
                       if (self->closable()) {
                         self->NotifyWindowCloseButtonClicked();
                       }
                       return TRUE;
                     }),
                     this);
    g_signal_connect(widget_, "notify::is-active",
                     G_CALLBACK(+[](GObject*, GParamSpec*, gpointer data) {
                       auto* self = static_cast<NativeWindowLinux*>(data);
                       if (self->IsFocused()) {
                         self->NotifyWindowFocus();
                       } else {
                         self->NotifyWindowBlur();
                       }
                     }),
                     this);
    g_signal_connect(
        widget_, "realize", G_CALLBACK(+[](GtkWidget*, gpointer data) {
          auto* self = static_cast<NativeWindowLinux*>(data);
          self->surface_ = gtk_native_get_surface(GTK_NATIVE(self->widget_));
          g_signal_connect(
              self->surface_, "notify::state",
              G_CALLBACK(+[](GObject*, GParamSpec*, gpointer data) {
                static_cast<NativeWindowLinux*>(data)->UpdateState();
              }),
              data);
          for (const char* signal : {"notify::width", "notify::height"}) {
            g_signal_connect(
                self->surface_, signal,
                G_CALLBACK(+[](GObject*, GParamSpec*, gpointer data) {
                  static_cast<NativeWindowLinux*>(data)->UpdateSize();
                }),
                data);
          }
          self->ApplyGeometry();
        }),
        this);
    g_signal_connect(
        widget_, "unrealize", G_CALLBACK(+[](GtkWidget*, gpointer data) {
          auto* self = static_cast<NativeWindowLinux*>(data);
          if (self->surface_) {
            g_signal_handlers_disconnect_by_data(self->surface_, self);
          }
          self->surface_ = nullptr;
        }),
        this);
  }

  ~NativeWindowLinux() override {
    CancelPendingResize();
    if (widget_) {
      if (surface_) {
        g_signal_handlers_disconnect_by_data(surface_, this);
      }
      g_signal_handlers_disconnect_by_data(widget_, this);
      gtk_window_destroy(GTK_WINDOW(widget_));
    }
  }
  void Close() override {
    if (closable()) {
      NotifyWindowCloseButtonClicked();
    }
  }
  void CloseImmediately() override {
    if (IsClosed()) {
      return;
    }
    CancelPendingResize();
    auto* widget = widget_;
    widget_ = nullptr;
    if (surface_) {
      g_signal_handlers_disconnect_by_data(surface_, this);
    }
    surface_ = nullptr;
    g_signal_handlers_disconnect_by_data(widget, this);
    gtk_window_destroy(GTK_WINDOW(widget));
    NotifyWindowClosed();
  }
  void Focus(bool focus) override {
    if (focus) {
      gtk_window_present(GTK_WINDOW(widget_));
    } else {
      Unsupported("focus(false)");
    }
  }
  bool IsFocused() override {
    return gtk_window_is_active(GTK_WINDOW(widget_));
  }
  void Show() override {
    gtk_window_present(GTK_WINDOW(widget_));
    NotifyWindowShow();
  }
  void ShowInactive() override {
    // Mapping without an activation request lets the compositor choose focus.
    gtk_widget_set_visible(widget_, TRUE);
    NotifyWindowShow();
  }
  void Hide() override {
    gtk_widget_set_visible(widget_, FALSE);
    NotifyWindowHide();
  }
  bool IsVisible() override {
    return widget_ && gtk_widget_get_visible(widget_);
  }
  bool IsEnabled() override { return gtk_widget_get_sensitive(widget_); }
  void SetEnabled(bool enable) override {
    gtk_widget_set_sensitive(widget_, enable);
  }
  void Maximize() override { gtk_window_maximize(GTK_WINDOW(widget_)); }
  void Unmaximize() override { gtk_window_unmaximize(GTK_WINDOW(widget_)); }
  bool IsMaximized() const override { return maximized_; }
  void Minimize() override { gtk_window_minimize(GTK_WINDOW(widget_)); }
  void Restore() override {
    gtk_window_unminimize(GTK_WINDOW(widget_));
    gtk_window_present(GTK_WINDOW(widget_));
  }
  bool IsMinimized() const override {
    if (surface_ && !GDK_IS_X11_SURFACE(surface_)) {
      if (!minimized_state_warning_) {
        minimized_state_warning_ = true;
        Unsupported("observed minimized state on Wayland");
      }
      return false;
    }
    return minimized_;
  }
  void SetFullScreen(bool fullscreen) override {
    if (fullscreen && !fullscreenable_) {
      return;
    }
    if (fullscreen) {
      gtk_window_fullscreen(GTK_WINDOW(widget_));
    } else {
      gtk_window_unfullscreen(GTK_WINDOW(widget_));
    }
  }
  bool IsFullscreen() const override { return fullscreen_; }
  void SetBounds(const gfx::Rect& bounds, bool animate) override {
    if (bounds.x() || bounds.y()) {
      Unsupported("absolute window positioning");
    }
    RequestSize(std::max(1, bounds.width()), std::max(1, bounds.height()));
    if (!gtk_widget_get_mapped(widget_)) {
      bounds_ = gfx::Rect(0, 0, std::max(1, bounds.width()),
                          std::max(1, bounds.height()));
      normal_bounds_ = bounds_;
    }
  }
  gfx::Rect GetBounds() const override {
    // GTK4 does not expose global toplevel coordinates on Wayland or X11.
    if (!gtk_widget_get_mapped(widget_) || gtk_widget_get_width(widget_) <= 1 ||
        gtk_widget_get_height(widget_) <= 1) {
      int width, height;
      gtk_window_get_default_size(GTK_WINDOW(widget_), &width, &height);
      return gfx::Rect(0, 0, std::max(1, width), std::max(1, height));
    }
    return gfx::Rect(0, 0, std::max(1, gtk_widget_get_width(widget_)),
                     std::max(1, gtk_widget_get_height(widget_)));
  }
  gfx::Size GetContentSize() const override {
    auto* content =
        GTK_WIDGET(g_object_get_data(G_OBJECT(widget_), "lynxtron-content"));
    const int menu_height = MenuHeight();
    int width = gtk_widget_get_width(content),
        height = gtk_widget_get_height(content);
    if (!gtk_widget_get_mapped(widget_) || width <= 1 || height <= 1) {
      gtk_window_get_default_size(GTK_WINDOW(widget_), &width, &height);
      height -= DecorationHeight(width);
    }
    return gfx::Size(std::max(1, width), std::max(1, height - menu_height));
  }
  void SetContentSize(const gfx::Size& size, bool) override {
    const int menu_height = MenuHeight();
    const int width = std::max(1, size.width());
    const int height =
        std::max(1, size.height() + menu_height + DecorationHeight(width));
    RequestSize(width, height);
  }
  gfx::Rect GetContentBounds() const override {
    return gfx::Rect(gfx::Point(), GetContentSize());
  }
  float GetDevicePixelRatio() const override {
    return surface_ ? gdk_surface_get_scale(surface_)
                    : gtk_widget_get_scale_factor(widget_);
  }
  gfx::Rect GetNormalBounds() const override { return normal_bounds_; }
  void SetResizable(bool resizable) override {
    set_resizable(resizable);
    gtk_window_set_resizable(GTK_WINDOW(widget_), resizable);
    ApplyFunctions();
  }
  void MoveTop() override { gtk_window_present(GTK_WINDOW(widget_)); }
  bool IsResizable() const override { return resizable(); }
  void SetMovable(bool movable) override {
    if (!movable) {
      Unsupported("disable compositor window movement");
    }
  }
  bool IsMovable() const override { return movable_; }
  void SetMinimizable(bool minimizable) override {
    if (!minimizable) {
      Unsupported("disable compositor minimization");
    }
  }
  bool IsMinimizable() const override { return minimizable(); }
  void SetMaximizable(bool maximizable) override {
    if (!maximizable) {
      Unsupported("disable compositor maximization");
    }
  }
  bool IsMaximizable() const override { return maximizable(); }
  void SetFullScreenable(bool fullscreenable) override {
    fullscreenable_ = fullscreenable;
  }
  bool IsFullScreenable() const override { return fullscreenable_; }
  void SetClosable(bool closable) override {
    set_closable(closable);
    ApplyFunctions();
  }
  bool IsClosable() const override { return closable(); }
  void SetAlwaysOnTop(ui::ZOrderLevel z_order,
                      const std::string& level,
                      int relativeLevel) override {
    z_order_ = z_order;
    Unsupported("always-on-top");
    z_order_ = ui::ZOrderLevel::kNormal;
  }
  ui::ZOrderLevel GetZOrderLevel() const override { return z_order_; }
  void Center() override { Unsupported("center"); }
  void SetTitle(const std::string& title) override {
    title_ = title;
    gtk_window_set_title(GTK_WINDOW(widget_), title.c_str());
  }
  std::string GetTitle() const override { return title_; }
  void FlashFrame(bool flash) override { Unsupported("flashFrame"); }
  void SetSkipTaskbar(bool skip) override { Unsupported("skipTaskbar"); }
  void SetExcludedFromShownWindowsMenu(bool excluded) override {
    excluded_from_shown_windows_menu_ = excluded;
  }
  bool IsExcludedFromShownWindowsMenu() override {
    return excluded_from_shown_windows_menu_;
  }
  void SetSimpleFullScreen(bool simple_fullscreen) override {
    simple_fullscreen_ = simple_fullscreen;
    SetFullScreen(simple_fullscreen);
  }
  bool IsSimpleFullScreen() override { return simple_fullscreen_; }
  void SetHasShadow(bool has_shadow) override {
    if (!has_shadow) {
      Unsupported("disable compositor shadow");
    }
  }
  bool HasShadow() override { return has_shadow_; }
  void SetOpacity(const double opacity) override {
    opacity_ = std::clamp(opacity, 0.0, 1.0);
    gtk_widget_set_opacity(widget_, opacity_);
  }
  double GetOpacity() override { return opacity_; }
  void SetFocusable(bool focusable) override {
    if (!focusable) {
      Unsupported("non-activatable toplevel");
    }
  }
  bool IsFocusable() const override { return focusable_; }
  void SetParentWindow(NativeWindow* parent) override {
    gtk_window_set_transient_for(
        GTK_WINDOW(widget_),
        parent ? GTK_WINDOW(parent->GetNativeWindowHandle()) : nullptr);
  }
  NativeWindowHandle GetNativeWindowHandle() const override { return widget_; }
  void SetProgressBar(double progress, const ProgressState state) override {}
  void SetVisibleOnAllWorkspaces(bool visible,
                                 bool visibleOnFullScreen,
                                 bool skipTransformProcessType) override {
    Unsupported("visibleOnAllWorkspaces");
    visible_on_all_workspaces_ = false;
  }
  bool IsVisibleOnAllWorkspaces() override {
    return visible_on_all_workspaces_;
  }

  void SetSizeConstraints(const SizeConstraints& constraints) override {
    NativeWindow::SetSizeConstraints(constraints);
    ApplyGeometry();
  }

 private:
  int MenuHeight() const {
    auto* menu =
        GTK_WIDGET(g_object_get_data(G_OBJECT(widget_), "lynxtron-menu"));
    if (!menu) {
      return 0;
    }
    int height = gtk_widget_get_height(menu);
    if (height <= 1) {
      gtk_widget_measure(menu, GTK_ORIENTATION_VERTICAL, -1, &height, nullptr,
                         nullptr, nullptr);
    }
    return std::max(0, height);
  }
  int DecorationHeight(int width) const {
    auto* content =
        GTK_WIDGET(g_object_get_data(G_OBJECT(widget_), "lynxtron-content"));
    // Native Wayland's client titlebar belongs to the window default size,
    // but not the content box. Measure even before the first allocation.
    int window_min = 0, content_min = 0;
    gtk_widget_measure(widget_, GTK_ORIENTATION_VERTICAL, width, &window_min,
                       nullptr, nullptr, nullptr);
    gtk_widget_measure(content, GTK_ORIENTATION_VERTICAL, width, &content_min,
                       nullptr, nullptr, nullptr);
    return std::max(0, window_min - content_min);
  }
  void CancelPendingResize() {
    if (resize_clock_) {
      g_signal_handler_disconnect(resize_clock_, resize_handler_);
      g_clear_object(&resize_clock_);
      resize_handler_ = 0;
    }
  }
  void RequestSize(int width, int height) {
    CancelPendingResize();
    gtk_window_set_default_size(GTK_WINDOW(widget_), width, height);
    if (!gtk_widget_get_mapped(widget_)) {
      bounds_ = gfx::Rect(0, 0, width, height);
      normal_bounds_ = bounds_;
      return;
    }
    pending_size_ = gfx::Size(width, height);
    // A configure already in flight can overwrite GTK's default size. Reapply
    // after GTK finishes the current layout/paint, retaining only the latest
    // request across both outer and content size setters.
    resize_clock_ =
        GDK_FRAME_CLOCK(g_object_ref(gtk_widget_get_frame_clock(widget_)));
    resize_handler_ = g_signal_connect_after(
        resize_clock_, "after-paint",
        G_CALLBACK(+[](GdkFrameClock*, gpointer data) {
          auto* self = static_cast<NativeWindowLinux*>(data);
          auto size = self->pending_size_;
          auto* widget = GTK_WIDGET(g_object_ref(self->widget_));
          self->CancelPendingResize();
          if (gtk_widget_get_realized(widget)) {
            gtk_window_set_default_size(GTK_WINDOW(widget), size.width(),
                                        size.height());
          }
          g_object_unref(widget);
        }),
        this);
    gdk_frame_clock_request_phase(resize_clock_,
                                  GDK_FRAME_CLOCK_PHASE_AFTER_PAINT);
  }
  static void Unsupported(const char* operation) {
    LOG(WARNING) << "GTK4 Linux capability is unavailable: " << operation;
  }
  void ApplyFunctions() {
    // GTK/GDK exposes resizability and deletability, not per-WM move/minimize
    // controls.
    if (surface_) {
      gdk_toplevel_set_deletable(GDK_TOPLEVEL(surface_), closable());
    }
  }
  void ApplyGeometry() {
    auto constraints = GetContentSizeConstraints();
    auto min = constraints.GetMinimumSize();
    auto* content =
        GTK_WIDGET(g_object_get_data(G_OBJECT(widget_), "lynxtron-content"));
    gtk_widget_set_size_request(
        content, constraints.HasMinimumSize() ? min.width() : -1,
        constraints.HasMinimumSize() ? min.height() : -1);
    if (constraints.HasMaximumSize()) {
      Unsupported("maximum size constraint");
    }
    ApplyFunctions();
  }
  void UpdateSize() {
    auto current = GetBounds();
    if (current.size() == bounds_.size()) {
      return;
    }
    bounds_ = current;
    if (!maximized_ && !fullscreen_) {
      normal_bounds_ = current;
    }
    NotifyWindowResize();
    NotifyWindowResized();
  }
  void UpdateState() {
    auto state = gdk_toplevel_get_state(GDK_TOPLEVEL(surface_));
    bool maximized = state & GDK_TOPLEVEL_STATE_MAXIMIZED;
    bool minimized = state & GDK_TOPLEVEL_STATE_MINIMIZED;
    bool fullscreen = state & GDK_TOPLEVEL_STATE_FULLSCREEN;
    if (maximized_ != maximized) {
      maximized_ = maximized;
      if (maximized) {
        NotifyWindowMaximize();
      } else {
        NotifyWindowUnmaximize();
      }
    }
    if (minimized_ != minimized) {
      minimized_ = minimized;
      if (minimized) {
        NotifyWindowMinimize();
      } else {
        NotifyWindowRestore();
      }
    }
    if (fullscreen_ != fullscreen) {
      fullscreen_ = fullscreen;
      if (fullscreen) {
        NotifyWindowEnterFullScreen();
      } else {
        NotifyWindowLeaveFullScreen();
      }
    }
  }
  gfx::Rect ContentBoundsToWindowBounds(
      const gfx::Rect& bounds) const override {
    auto outer = GetBounds();
    auto content = GetContentBounds();
    return gfx::Rect(bounds.x() - content.x() + outer.x(),
                     bounds.y() - content.y() + outer.y(),
                     bounds.width() - content.width() + outer.width(),
                     bounds.height() - content.height() + outer.height());
  }
  gfx::Rect WindowBoundsToContentBounds(
      const gfx::Rect& bounds) const override {
    auto outer = GetBounds();
    auto content = GetContentBounds();
    return gfx::Rect(
        bounds.x() - outer.x() + content.x(),
        bounds.y() - outer.y() + content.y(),
        std::max(1, bounds.width() - outer.width() + content.width()),
        std::max(1, bounds.height() - outer.height() + content.height()));
  }

  GdkFrameClock* resize_clock_ = nullptr;
  gulong resize_handler_ = 0;
  gfx::Size pending_size_;
  GtkWidget* widget_ = nullptr;
  GdkSurface* surface_ = nullptr;
  gfx::Rect normal_bounds_;
  gfx::Rect bounds_;
  std::string title_;
  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;
  double opacity_ = 1.0;
  bool enabled_ = true;
  bool visible_ = false;
  bool focused_ = false;
  bool maximized_ = false;
  mutable bool minimized_state_warning_ = false;
  bool minimized_ = false;
  bool fullscreen_ = false;
  bool movable_ = true;
  bool fullscreenable_ = true;
  bool excluded_from_shown_windows_menu_ = false;
  bool simple_fullscreen_ = false;
  bool has_shadow_ = true;
  bool focusable_ = true;
  bool visible_on_all_workspaces_ = false;
};

}  // namespace

NativeWindow* NativeWindow::Create(const gin_helper::Dictionary& options,
                                   NativeWindow* parent) {
  return new NativeWindowLinux(options, parent);
}

}  // namespace lynxtron
