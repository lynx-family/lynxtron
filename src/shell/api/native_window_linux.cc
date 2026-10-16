// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/native_window.h"

#include <gtk/gtk.h>

#include <algorithm>

#include "base/check.h"
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
    widget_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    auto* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(widget_), content);
    g_object_set_data(G_OBJECT(widget_), "lynxtron-content", content);
    ApplyLinuxApplicationMenu(this);
    gtk_window_set_default_size(GTK_WINDOW(widget_), width(), height());
    gtk_window_set_decorated(GTK_WINDOW(widget_), frame());
    gtk_window_set_resizable(GTK_WINDOW(widget_), resizable());
    gtk_window_set_modal(GTK_WINDOW(widget_), is_modal());
    SetParentWindow(parent);
    g_signal_connect(widget_, "realize",
                     G_CALLBACK(+[](GtkWidget*, gpointer data) {
                       static_cast<NativeWindowLinux*>(data)->ApplyFunctions();
                       static_cast<NativeWindowLinux*>(data)->ApplyGeometry();
                     }),
                     this);
    g_signal_connect(
        widget_, "delete-event",
        G_CALLBACK(+[](GtkWidget*, GdkEvent*, gpointer data) -> gboolean {
          auto* self = static_cast<NativeWindowLinux*>(data);
          if (self->closable()) {
            self->NotifyWindowCloseButtonClicked();
          }
          return TRUE;
        }),
        this);
    g_signal_connect(
        widget_, "focus-in-event",
        G_CALLBACK(+[](GtkWidget*, GdkEvent*, gpointer data) -> gboolean {
          static_cast<NativeWindowLinux*>(data)->NotifyWindowFocus();
          return FALSE;
        }),
        this);
    g_signal_connect(
        widget_, "focus-out-event",
        G_CALLBACK(+[](GtkWidget*, GdkEvent*, gpointer data) -> gboolean {
          static_cast<NativeWindowLinux*>(data)->NotifyWindowBlur();
          return FALSE;
        }),
        this);
    g_signal_connect(widget_, "configure-event",
                     G_CALLBACK(+[](GtkWidget*, GdkEventConfigure* event,
                                    gpointer data) -> gboolean {
                       auto* self = static_cast<NativeWindowLinux*>(data);
                       bool moved = self->bounds_.x() != event->x ||
                                    self->bounds_.y() != event->y;
                       bool resized = self->bounds_.width() != event->width ||
                                      self->bounds_.height() != event->height;
                       self->bounds_ = gfx::Rect(event->x, event->y,
                                                 event->width, event->height);
                       if (!self->maximized_ && !self->fullscreen_) {
                         self->normal_bounds_ = self->GetBounds();
                       }
                       if (resized) {
                         self->NotifyWindowResize();
                         self->NotifyWindowResized();
                       }
                       if (moved) {
                         self->NotifyWindowMove();
                         self->NotifyWindowMoved();
                       }
                       return FALSE;
                     }),
                     this);
    g_signal_connect(
        widget_, "window-state-event",
        G_CALLBACK(+[](GtkWidget*, GdkEventWindowState* event,
                       gpointer data) -> gboolean {
          auto* self = static_cast<NativeWindowLinux*>(data);
          bool maximized = event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED;
          bool minimized = event->new_window_state & GDK_WINDOW_STATE_ICONIFIED;
          bool fullscreen =
              event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;
          if (maximized != self->maximized_) {
            self->maximized_ = maximized;
            if (maximized) {
              self->NotifyWindowMaximize();
            } else {
              self->NotifyWindowUnmaximize();
            }
          }
          if (minimized != self->minimized_) {
            self->minimized_ = minimized;
            if (minimized) {
              self->NotifyWindowMinimize();
            } else {
              self->NotifyWindowRestore();
            }
          }
          if (fullscreen != self->fullscreen_) {
            self->fullscreen_ = fullscreen;
            if (fullscreen) {
              self->NotifyWindowEnterFullScreen();
            } else {
              self->NotifyWindowLeaveFullScreen();
            }
          }
          return FALSE;
        }),
        this);
  }

  ~NativeWindowLinux() override {
    if (widget_) {
      g_signal_handlers_disconnect_by_data(widget_, this);
      gtk_widget_destroy(widget_);
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
    auto* widget = widget_;
    widget_ = nullptr;
    g_signal_handlers_disconnect_by_data(widget, this);
    gtk_widget_destroy(widget);
    NotifyWindowClosed();
  }
  void Focus(bool focus) override {
    if (focus) {
      gtk_window_present(GTK_WINDOW(widget_));
    } else if (auto* root = gdk_get_default_root_window()) {
      gdk_window_focus(root, GDK_CURRENT_TIME);
    }
  }
  bool IsFocused() override {
    return gtk_window_is_active(GTK_WINDOW(widget_));
  }
  void Show() override {
    gtk_widget_show_all(widget_);
    NotifyWindowShow();
  }
  void ShowInactive() override {
    gtk_window_set_focus_on_map(GTK_WINDOW(widget_), FALSE);
    Show();
    gtk_window_set_focus_on_map(GTK_WINDOW(widget_), focusable_);
  }
  void Hide() override {
    gtk_widget_hide(widget_);
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
  void Minimize() override { gtk_window_iconify(GTK_WINDOW(widget_)); }
  void Restore() override { gtk_window_deiconify(GTK_WINDOW(widget_)); }
  bool IsMinimized() const override { return minimized_; }
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
    gtk_window_move(GTK_WINDOW(widget_), bounds.x(), bounds.y());
    int width, height;
    gtk_window_get_size(GTK_WINDOW(widget_), &width, &height);
    auto outer = GetBounds();
    gtk_window_resize(GTK_WINDOW(widget_),
                      std::max(1, bounds.width() - outer.width() + width),
                      std::max(1, bounds.height() - outer.height() + height));
  }
  gfx::Rect GetBounds() const override {
    if (auto* native = gtk_widget_get_window(widget_)) {
      GdkRectangle rect;
      gdk_window_get_frame_extents(native, &rect);
      return gfx::Rect(rect.x, rect.y, rect.width, rect.height);
    }
    return bounds_;
  }
  gfx::Size GetContentSize() const override {
    int menu_height = 0;
    if (auto* menu =
            GTK_WIDGET(g_object_get_data(G_OBJECT(widget_), "lynxtron-menu"))) {
      menu_height = gtk_widget_get_allocated_height(menu);
    }
    int width, height;
    gtk_window_get_size(GTK_WINDOW(widget_), &width, &height);
    return gfx::Size(width, std::max(1, height - menu_height));
  }
  void SetContentSize(const gfx::Size& size, bool) override {
    int menu_height = 0;
    if (auto* menu =
            GTK_WIDGET(g_object_get_data(G_OBJECT(widget_), "lynxtron-menu"))) {
      menu_height = gtk_widget_get_allocated_height(menu);
    }
    gtk_window_resize(GTK_WINDOW(widget_), std::max(1, size.width()),
                      std::max(1, size.height() + menu_height));
  }
  gfx::Rect GetContentBounds() const override {
    int x = bounds_.x(), y = bounds_.y();
    if (auto* native = gtk_widget_get_window(widget_)) {
      gdk_window_get_origin(native, &x, &y);
    }
    if (auto* menu =
            GTK_WIDGET(g_object_get_data(G_OBJECT(widget_), "lynxtron-menu"))) {
      y += gtk_widget_get_allocated_height(menu);
    }
    return gfx::Rect(gfx::Point(x, y), GetContentSize());
  }
  float GetDevicePixelRatio() const override {
    return gtk_widget_get_scale_factor(widget_);
  }
  gfx::Rect GetNormalBounds() const override { return normal_bounds_; }
  void SetResizable(bool resizable) override {
    set_resizable(resizable);
    gtk_window_set_resizable(GTK_WINDOW(widget_), resizable);
    ApplyFunctions();
  }
  void MoveTop() override {
    if (auto* window = gtk_widget_get_window(widget_)) {
      gdk_window_raise(window);
    }
  }
  bool IsResizable() const override { return resizable(); }
  void SetMovable(bool movable) override {
    movable_ = movable;
    ApplyFunctions();
  }
  bool IsMovable() const override { return movable_; }
  void SetMinimizable(bool minimizable) override {
    set_minimizable(minimizable);
    ApplyFunctions();
  }
  bool IsMinimizable() const override { return minimizable(); }
  void SetMaximizable(bool maximizable) override {
    set_maximizable(maximizable);
    ApplyFunctions();
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
    gtk_window_set_keep_above(GTK_WINDOW(widget_),
                              z_order != ui::ZOrderLevel::kNormal);
    NotifyWindowAlwaysOnTopChanged();
  }
  ui::ZOrderLevel GetZOrderLevel() const override { return z_order_; }
  void Center() override {
    gtk_window_set_position(GTK_WINDOW(widget_), GTK_WIN_POS_CENTER);
  }
  void SetTitle(const std::string& title) override {
    title_ = title;
    gtk_window_set_title(GTK_WINDOW(widget_), title.c_str());
  }
  std::string GetTitle() const override { return title_; }
  void FlashFrame(bool flash) override {
    gtk_window_set_urgency_hint(GTK_WINDOW(widget_), flash);
  }
  void SetSkipTaskbar(bool skip) override {
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(widget_), skip);
  }
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
  void SetHasShadow(bool has_shadow) override { has_shadow_ = has_shadow; }
  bool HasShadow() override { return has_shadow_; }
  void SetOpacity(const double opacity) override {
    opacity_ = std::clamp(opacity, 0.0, 1.0);
    gtk_widget_set_opacity(widget_, opacity_);
  }
  double GetOpacity() override { return opacity_; }
  void SetFocusable(bool focusable) override {
    focusable_ = focusable;
    gtk_window_set_accept_focus(GTK_WINDOW(widget_), focusable);
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
    visible_on_all_workspaces_ = visible;
    if (visible) {
      gtk_window_stick(GTK_WINDOW(widget_));
    } else {
      gtk_window_unstick(GTK_WINDOW(widget_));
    }
  }
  bool IsVisibleOnAllWorkspaces() override {
    return visible_on_all_workspaces_;
  }

  void SetSizeConstraints(const SizeConstraints& constraints) override {
    NativeWindow::SetSizeConstraints(constraints);
    ApplyGeometry();
  }

 private:
  void ApplyFunctions() {
    if (auto* window = gtk_widget_get_window(widget_)) {
      int functions = (movable_ ? GDK_FUNC_MOVE : 0) |
                      (resizable() ? GDK_FUNC_RESIZE : 0) |
                      (minimizable() ? GDK_FUNC_MINIMIZE : 0) |
                      (maximizable() ? GDK_FUNC_MAXIMIZE : 0) |
                      (closable() ? GDK_FUNC_CLOSE : 0);
      gdk_window_set_functions(window, GdkWMFunction(functions));
    }
  }
  void ApplyGeometry() {
    auto constraints = GetSizeConstraints();
    auto min = constraints.GetMinimumSize();
    auto max = constraints.GetMaximumSize();
    GdkGeometry geometry{};
    int flags = 0;
    int width, height;
    gtk_window_get_size(GTK_WINDOW(widget_), &width, &height);
    auto outer = GetBounds();
    if (constraints.HasMinimumSize()) {
      geometry.min_width = std::max(1, min.width() - outer.width() + width);
      geometry.min_height = std::max(1, min.height() - outer.height() + height);
      flags |= GDK_HINT_MIN_SIZE;
    }
    if (constraints.HasMaximumSize()) {
      geometry.max_width =
          max.width() ? std::max(1, max.width() - outer.width() + width)
                      : G_MAXINT;
      geometry.max_height =
          max.height() ? std::max(1, max.height() - outer.height() + height)
                       : G_MAXINT;
      flags |= GDK_HINT_MAX_SIZE;
    }
    gtk_window_set_geometry_hints(GTK_WINDOW(widget_), nullptr, &geometry,
                                  GdkWindowHints(flags));
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

  GtkWidget* widget_ = nullptr;
  gfx::Rect normal_bounds_;
  gfx::Rect bounds_;
  std::string title_;
  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;
  double opacity_ = 1.0;
  bool enabled_ = true;
  bool visible_ = false;
  bool focused_ = false;
  bool maximized_ = false;
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
