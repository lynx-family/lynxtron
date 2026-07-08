// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/native_window_windowless.h"

#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/options_switches.h"

namespace lynxtron {

NativeWindowWindowless::NativeWindowWindowless(
    const gin_helper::Dictionary& options,
    NativeWindow* parent)
    : NativeWindow(options, parent), bounds_(0, 0, width(), height()) {
  device_pixel_ratio_ =
      options.ValueOrDefault(options::kDeviceScaleFactor, 1.0);
}

NativeWindowWindowless::~NativeWindowWindowless() = default;

void NativeWindowWindowless::Close() {
  CloseImmediately();
}

void NativeWindowWindowless::CloseImmediately() {
  NotifyWindowClosed();
}

bool NativeWindowWindowless::IsWindowless() const {
  return true;
}

void NativeWindowWindowless::Focus(bool focus) {
  focused_ = focus;
  if (focus) {
    NotifyWindowFocus();
  } else {
    NotifyWindowBlur();
  }
}

bool NativeWindowWindowless::IsFocused() {
  return focused_;
}

void NativeWindowWindowless::Show() {
  visible_ = true;
  NotifyWindowShow();
}

void NativeWindowWindowless::ShowInactive() {
  Show();
}

void NativeWindowWindowless::Hide() {
  visible_ = false;
  NotifyWindowHide();
}

bool NativeWindowWindowless::IsVisible() {
  return visible_;
}

bool NativeWindowWindowless::IsEnabled() {
  return enabled_;
}

void NativeWindowWindowless::SetEnabled(bool enable) {
  enabled_ = enable;
}

void NativeWindowWindowless::Maximize() {
  maximized_ = true;
  NotifyWindowMaximize();
}

void NativeWindowWindowless::Unmaximize() {
  maximized_ = false;
  NotifyWindowUnmaximize();
}

bool NativeWindowWindowless::IsMaximized() const {
  return maximized_;
}

void NativeWindowWindowless::Minimize() {
  minimized_ = true;
  NotifyWindowMinimize();
}

void NativeWindowWindowless::Restore() {
  minimized_ = false;
  NotifyWindowRestore();
}

bool NativeWindowWindowless::IsMinimized() const {
  return minimized_;
}

void NativeWindowWindowless::SetFullScreen(bool fullscreen) {
  fullscreen_ = fullscreen;
  if (fullscreen) {
    NotifyWindowEnterFullScreen();
  } else {
    NotifyWindowLeaveFullScreen();
  }
}

bool NativeWindowWindowless::IsFullscreen() const {
  return fullscreen_;
}

void NativeWindowWindowless::SetBounds(const gfx::Rect& bounds, bool animate) {
  bounds_ = bounds;
  NotifyWindowResize();
  NotifyWindowResized();
  NotifyWindowMove();
  NotifyWindowMoved();
}

gfx::Rect NativeWindowWindowless::GetBounds() const {
  return bounds_;
}

float NativeWindowWindowless::GetDevicePixelRatio() const {
  return device_pixel_ratio_;
}

gfx::Rect NativeWindowWindowless::GetNormalBounds() const {
  return bounds_;
}

void NativeWindowWindowless::SetResizable(bool resizable) {
  set_resizable(resizable);
}

void NativeWindowWindowless::MoveTop() {}

bool NativeWindowWindowless::IsResizable() const {
  return resizable();
}

void NativeWindowWindowless::SetMovable(bool movable) {
  movable_ = movable;
}

bool NativeWindowWindowless::IsMovable() const {
  return movable_;
}

void NativeWindowWindowless::SetMinimizable(bool minimizable) {
  set_minimizable(minimizable);
}

bool NativeWindowWindowless::IsMinimizable() const {
  return minimizable();
}

void NativeWindowWindowless::SetMaximizable(bool maximizable) {
  set_maximizable(maximizable);
}

bool NativeWindowWindowless::IsMaximizable() const {
  return maximizable();
}

void NativeWindowWindowless::SetFullScreenable(bool fullscreenable) {
  fullscreenable_ = fullscreenable;
}

bool NativeWindowWindowless::IsFullScreenable() const {
  return fullscreenable_;
}

void NativeWindowWindowless::SetClosable(bool closable) {
  set_closable(closable);
}

bool NativeWindowWindowless::IsClosable() const {
  return closable();
}

void NativeWindowWindowless::SetAlwaysOnTop(ui::ZOrderLevel z_order,
                                            const std::string& level,
                                            int relativeLevel) {
  z_order_ = z_order;
  NotifyWindowAlwaysOnTopChanged();
}

ui::ZOrderLevel NativeWindowWindowless::GetZOrderLevel() const {
  return z_order_;
}

void NativeWindowWindowless::Center() {}

void NativeWindowWindowless::SetTitle(const std::string& title) {
  title_ = title;
}

std::string NativeWindowWindowless::GetTitle() const {
  return title_;
}

#if BUILDFLAG(IS_MAC)
std::string NativeWindowWindowless::GetAlwaysOnTopLevel() {
  return "normal";
}

void NativeWindowWindowless::SetActive(bool is_key) {
  active_ = is_key;
}

bool NativeWindowWindowless::IsActive() const {
  return active_;
}

gfx::NativeWindow NativeWindowWindowless::GetNativeWindow() const {
  return gfx::NativeWindow();
}

void NativeWindowWindowless::SetWindowButtonVisibility(bool visible) {
  window_button_visible_ = visible;
}

bool NativeWindowWindowless::GetWindowButtonVisibility() const {
  return window_button_visible_;
}

void NativeWindowWindowless::SetTrafficLightPosition(
    std::optional<gfx::Point> position) {
  traffic_light_position_ = position;
}

std::optional<gfx::Point> NativeWindowWindowless::GetTrafficLightPosition()
    const {
  return traffic_light_position_;
}

void NativeWindowWindowless::RedrawTrafficLights() {}

void NativeWindowWindowless::UpdateFrame() {}

bool NativeWindowWindowless::IsHiddenInMissionControl() const {
  return hidden_in_mission_control_;
}

void NativeWindowWindowless::SetHiddenInMissionControl(bool hidden) {
  hidden_in_mission_control_ = hidden;
}
#endif

void NativeWindowWindowless::FlashFrame(bool flash) {}

void NativeWindowWindowless::SetSkipTaskbar(bool skip) {}

void NativeWindowWindowless::SetExcludedFromShownWindowsMenu(bool excluded) {
  excluded_from_shown_windows_menu_ = excluded;
}

bool NativeWindowWindowless::IsExcludedFromShownWindowsMenu() {
  return excluded_from_shown_windows_menu_;
}

void NativeWindowWindowless::SetSimpleFullScreen(bool simple_fullscreen) {
  simple_fullscreen_ = simple_fullscreen;
}

bool NativeWindowWindowless::IsSimpleFullScreen() {
  return simple_fullscreen_;
}

void NativeWindowWindowless::SetHasShadow(bool has_shadow) {
  has_shadow_ = has_shadow;
}

bool NativeWindowWindowless::HasShadow() {
  return has_shadow_;
}

void NativeWindowWindowless::SetOpacity(const double opacity) {
  opacity_ = opacity;
}

double NativeWindowWindowless::GetOpacity() {
  return opacity_;
}

void NativeWindowWindowless::SetFocusable(bool focusable) {
  focusable_ = focusable;
}

bool NativeWindowWindowless::IsFocusable() const {
  return focusable_;
}

void NativeWindowWindowless::SetParentWindow(NativeWindow* parent) {}

NativeWindowHandle NativeWindowWindowless::GetNativeWindowHandle() const {
  return nullptr;
}

void NativeWindowWindowless::SetProgressBar(double progress,
                                            const ProgressState state) {}

void NativeWindowWindowless::SetVisibleOnAllWorkspaces(
    bool visible,
    bool visibleOnFullScreen,
    bool skipTransformProcessType) {
  visible_on_all_workspaces_ = visible;
}

bool NativeWindowWindowless::IsVisibleOnAllWorkspaces() {
  return visible_on_all_workspaces_;
}

gfx::Rect NativeWindowWindowless::ContentBoundsToWindowBounds(
    const gfx::Rect& bounds) const {
  return bounds;
}

gfx::Rect NativeWindowWindowless::WindowBoundsToContentBounds(
    const gfx::Rect& bounds) const {
  return bounds;
}

}  // namespace lynxtron
