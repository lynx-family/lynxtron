// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/native_window.h"

namespace lynxtron {

namespace {

class NativeWindowLinux : public NativeWindow {
 public:
  NativeWindowLinux(const gin_helper::Dictionary& options, NativeWindow* parent)
      : NativeWindow(options, parent), bounds_(0, 0, width(), height()) {}

  void Close() override { CloseImmediately(); }
  void CloseImmediately() override {}
  void Focus(bool focus) override { focused_ = focus; }
  bool IsFocused() override { return focused_; }
  void Show() override { visible_ = true; }
  void ShowInactive() override { visible_ = true; }
  void Hide() override { visible_ = false; }
  bool IsVisible() override { return visible_; }
  bool IsEnabled() override { return enabled_; }
  void SetEnabled(bool enable) override { enabled_ = enable; }
  void Maximize() override { maximized_ = true; }
  void Unmaximize() override { maximized_ = false; }
  bool IsMaximized() const override { return maximized_; }
  void Minimize() override { minimized_ = true; }
  void Restore() override { minimized_ = false; }
  bool IsMinimized() const override { return minimized_; }
  void SetFullScreen(bool fullscreen) override { fullscreen_ = fullscreen; }
  bool IsFullscreen() const override { return fullscreen_; }
  void SetBounds(const gfx::Rect& bounds, bool animate) override {
    bounds_ = bounds;
  }
  gfx::Rect GetBounds() const override { return bounds_; }
  float GetDevicePixelRatio() const override { return 1.0f; }
  gfx::Rect GetNormalBounds() const override { return bounds_; }
  void SetResizable(bool resizable) override { set_resizable(resizable); }
  void MoveTop() override {}
  bool IsResizable() const override { return resizable(); }
  void SetMovable(bool movable) override { movable_ = movable; }
  bool IsMovable() const override { return movable_; }
  void SetMinimizable(bool minimizable) override {
    set_minimizable(minimizable);
  }
  bool IsMinimizable() const override { return minimizable(); }
  void SetMaximizable(bool maximizable) override {
    set_maximizable(maximizable);
  }
  bool IsMaximizable() const override { return maximizable(); }
  void SetFullScreenable(bool fullscreenable) override {
    fullscreenable_ = fullscreenable;
  }
  bool IsFullScreenable() const override { return fullscreenable_; }
  void SetClosable(bool closable) override { set_closable(closable); }
  bool IsClosable() const override { return closable(); }
  void SetAlwaysOnTop(ui::ZOrderLevel z_order,
                      const std::string& level,
                      int relativeLevel) override {
    z_order_ = z_order;
  }
  ui::ZOrderLevel GetZOrderLevel() const override { return z_order_; }
  void Center() override {}
  void SetTitle(const std::string& title) override { title_ = title; }
  std::string GetTitle() const override { return title_; }
  void FlashFrame(bool flash) override {}
  void SetSkipTaskbar(bool skip) override {}
  void SetExcludedFromShownWindowsMenu(bool excluded) override {
    excluded_from_shown_windows_menu_ = excluded;
  }
  bool IsExcludedFromShownWindowsMenu() override {
    return excluded_from_shown_windows_menu_;
  }
  void SetSimpleFullScreen(bool simple_fullscreen) override {
    simple_fullscreen_ = simple_fullscreen;
  }
  bool IsSimpleFullScreen() override { return simple_fullscreen_; }
  void SetHasShadow(bool has_shadow) override { has_shadow_ = has_shadow; }
  bool HasShadow() override { return has_shadow_; }
  void SetOpacity(const double opacity) override { opacity_ = opacity; }
  double GetOpacity() override { return opacity_; }
  void SetFocusable(bool focusable) override { focusable_ = focusable; }
  bool IsFocusable() const override { return focusable_; }
  void SetParentWindow(NativeWindow* parent) override {}
  NativeWindowHandle GetNativeWindowHandle() const override { return nullptr; }
  void SetProgressBar(double progress, const ProgressState state) override {}
  void SetVisibleOnAllWorkspaces(bool visible,
                                 bool visibleOnFullScreen,
                                 bool skipTransformProcessType) override {
    visible_on_all_workspaces_ = visible;
  }
  bool IsVisibleOnAllWorkspaces() override {
    return visible_on_all_workspaces_;
  }

 private:
  gfx::Rect ContentBoundsToWindowBounds(
      const gfx::Rect& bounds) const override {
    return bounds;
  }
  gfx::Rect WindowBoundsToContentBounds(
      const gfx::Rect& bounds) const override {
    return bounds;
  }

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
