// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_APP_NATIVE_WINDOW_WINDOWLESS_H_
#define LYNXTRON_SHELL_APP_NATIVE_WINDOW_WINDOWLESS_H_

#include <optional>
#include <string>

#include "shell/app/native_window.h"

namespace lynxtron {

class NativeWindowWindowless : public NativeWindow {
 public:
  NativeWindowWindowless(const gin_helper::Dictionary& options,
                         NativeWindow* parent);
  ~NativeWindowWindowless() override;

  void Close() override;
  void CloseImmediately() override;
  bool IsWindowless() const override;
  void Focus(bool focus) override;
  bool IsFocused() override;
  void Show() override;
  void ShowInactive() override;
  void Hide() override;
  bool IsVisible() override;
  bool IsEnabled() override;
  void SetEnabled(bool enable) override;
  void Maximize() override;
  void Unmaximize() override;
  bool IsMaximized() const override;
  void Minimize() override;
  void Restore() override;
  bool IsMinimized() const override;
  void SetFullScreen(bool fullscreen) override;
  bool IsFullscreen() const override;
  void SetBounds(const gfx::Rect& bounds, bool animate) override;
  gfx::Rect GetBounds() const override;
  float GetDevicePixelRatio() const override;
  gfx::Rect GetNormalBounds() const override;
  void SetResizable(bool resizable) override;
  void MoveTop() override;
  bool IsResizable() const override;
  void SetMovable(bool movable) override;
  bool IsMovable() const override;
  void SetMinimizable(bool minimizable) override;
  bool IsMinimizable() const override;
  void SetMaximizable(bool maximizable) override;
  bool IsMaximizable() const override;
  void SetFullScreenable(bool fullscreenable) override;
  bool IsFullScreenable() const override;
  void SetClosable(bool closable) override;
  bool IsClosable() const override;
  void SetAlwaysOnTop(ui::ZOrderLevel z_order,
                      const std::string& level = "floating",
                      int relativeLevel = 0) override;
  ui::ZOrderLevel GetZOrderLevel() const override;
  void Center() override;
  void SetTitle(const std::string& title) override;
  std::string GetTitle() const override;
#if BUILDFLAG(IS_MAC)
  std::string GetAlwaysOnTopLevel() override;
  void SetActive(bool is_key) override;
  bool IsActive() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  void SetWindowButtonVisibility(bool visible) override;
  bool GetWindowButtonVisibility() const override;
  void SetTrafficLightPosition(std::optional<gfx::Point> position) override;
  std::optional<gfx::Point> GetTrafficLightPosition() const override;
  void RedrawTrafficLights() override;
  void UpdateFrame() override;
  bool IsHiddenInMissionControl() const override;
  void SetHiddenInMissionControl(bool hidden) override;
#endif
  void FlashFrame(bool flash) override;
  void SetSkipTaskbar(bool skip) override;
  void SetExcludedFromShownWindowsMenu(bool excluded) override;
  bool IsExcludedFromShownWindowsMenu() override;
  void SetSimpleFullScreen(bool simple_fullscreen) override;
  bool IsSimpleFullScreen() override;
  void SetHasShadow(bool has_shadow) override;
  bool HasShadow() override;
  void SetOpacity(const double opacity) override;
  double GetOpacity() override;
  void SetFocusable(bool focusable) override;
  bool IsFocusable() const override;
  void SetParentWindow(NativeWindow* parent) override;
  NativeWindowHandle GetNativeWindowHandle() const override;
  void SetProgressBar(double progress, const ProgressState state) override;
  void SetVisibleOnAllWorkspaces(bool visible,
                                 bool visibleOnFullScreen,
                                 bool skipTransformProcessType) override;
  bool IsVisibleOnAllWorkspaces() override;

 private:
  gfx::Rect ContentBoundsToWindowBounds(const gfx::Rect& bounds) const override;
  gfx::Rect WindowBoundsToContentBounds(const gfx::Rect& bounds) const override;

  gfx::Rect bounds_;
  std::string title_;
  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;
  double opacity_ = 1.0;
  float device_pixel_ratio_ = 1.0f;
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
#if BUILDFLAG(IS_MAC)
  bool active_ = false;
  bool window_button_visible_ = true;
  bool hidden_in_mission_control_ = false;
  std::optional<gfx::Point> traffic_light_position_;
#endif
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_NATIVE_WINDOW_WINDOWLESS_H_
