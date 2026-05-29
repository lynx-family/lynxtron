// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/app/native_window.h"

#include <algorithm>
#include <climits>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "shell/app/application.h"
#include "shell/app/window_list.h"
#include "shell/common/color_parser.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/persistent_dictionary.h"
#include "shell/common/options_switches.h"

namespace lynxtron {
NativeWindow::NativeWindow(const gin_helper::Dictionary& options,
                           NativeWindow* parent)
    : parent_(parent) {
  frame_ = options.ValueOrDefault(options::kFrame, true);
  transparent_ = options.ValueOrDefault(options::kTransparent, false);
  resizable_ = options.ValueOrDefault(options::kResizable, true);
  width_ = options.ValueOrDefault(options::kWidth, 800);
  height_ = options.ValueOrDefault(options::kHeight, 600);
  use_content_size_ = options.ValueOrDefault(options::kUseContentSize, false);
  minimizable_ = options.ValueOrDefault(options::kMinimizable, true);
  maximizable_ = options.ValueOrDefault(options::kMaximizable, true);
  closable_ = options.ValueOrDefault(options::kClosable, true);
  window_type_ = options.ValueOrDefault(options::kType, std::string{});

  if (parent) {
    options.Get("modal", &is_modal_);
  }

  // Match Electron: modal child windows default to non-minimizable unless the
  // caller explicitly specifies otherwise.
  if (parent && is_modal_ && !options.Has(options::kMinimizable)) {
    minimizable_ = false;
  }

  WindowList::AddWindow(this);
}

NativeWindow::~NativeWindow() {
  NotifyWindowClosed();
}

void NativeWindow::InitFromOptions(const gin_helper::Dictionary& options) {
  // Setup window from options.
  int x = 0;
  int y = 0;
  const bool has_x = options.Get(options::kX, &x);
  const bool has_y = options.Get(options::kY, &y);
  if (has_x && has_y) {
    SetPosition(gfx::Point{x, y}, false);
  } else if (!has_x && !has_y &&
             options.ValueOrDefault(options::kCenter, true)) {
    Center();
  }

  // On Linux and Window we may already have maximum size defined.
  SizeConstraints size_constraints(
      use_content_size_ ? GetContentSizeConstraints() : GetSizeConstraints());

  const int min_width = options.ValueOrDefault(
      options::kMinWidth, size_constraints.GetMinimumSize().width());
  const int min_height = options.ValueOrDefault(
      options::kMinHeight, size_constraints.GetMinimumSize().height());
  size_constraints.set_minimum_size(gfx::Size(min_width, min_height));

  gfx::Size max_size = size_constraints.GetMaximumSize();
  int max_width = max_size.width() > 0 ? max_size.width() : INT_MAX;
  int max_height = max_size.height() > 0 ? max_size.height() : INT_MAX;
  bool have_max_width = options.Get(options::kMaxWidth, &max_width);
  if (have_max_width && max_width <= 0) {
    max_width = INT_MAX;
  }
  bool have_max_height = options.Get(options::kMaxHeight, &max_height);
  if (have_max_height && max_height <= 0) {
    max_height = INT_MAX;
  }

  // By default the window has a default maximum size that prevents it
  // from being resized larger than the screen, so we should only set this
  // if the user has passed in values.
  if (have_max_height || have_max_width || !max_size.IsEmpty()) {
    size_constraints.set_maximum_size(gfx::Size(max_width, max_height));
  }

  if (use_content_size_) {
    SetContentSizeConstraints(size_constraints);
  } else {
    SetSizeConstraints(size_constraints);
  }

  if (bool val; options.Get(options::kMovable, &val)) {
    SetMovable(val);
  }

  if (bool val; options.Get(options::kHasShadow, &val)) {
    SetHasShadow(val);
  }

  if (double val; options.Get(options::kOpacity, &val)) {
    SetOpacity(val);
  }

  if (std::string background_color;
      options.Get(options::kBackgroundColor, &background_color)) {
    SkColor color = background_color_;
    if (content::ParseCssColorString(background_color, &color)) {
      SetBackgroundColor(color);
    }
  }

  if (bool val; options.Get(options::kAlwaysOnTop, &val) && val) {
    SetAlwaysOnTop(ui::ZOrderLevel::kFloatingWindow);
  }

  bool fullscreenable = true;
  bool fullscreen = false;
  if (options.Get(options::kFullscreen, &fullscreen) && !fullscreen) {
    // Disable fullscreen button if 'fullscreen' is specified to false.
#if BUILDFLAG(IS_MAC)
    fullscreenable = false;
#endif
  }

  options.Get(options::kFullScreenable, &fullscreenable);
  SetFullScreenable(fullscreenable);

  if (fullscreen) {
    SetFullScreen(true);
  }

  if (bool val; options.Get(options::kResizable, &val)) {
    SetResizable(val);
  }

  if (bool val; options.Get(options::kSkipTaskbar, &val)) {
    SetSkipTaskbar(val);
  }

  if (bool val; options.Get(options::kFocusable, &val)) {
    SetFocusable(val);
  }

  SetTitle(
      options.ValueOrDefault(options::kTitle, Application::Get()->GetName()));

  // Then show it.
  if (options.ValueOrDefault(options::kShow, true)) {
    Show();
  }
}

bool NativeWindow::IsClosed() const {
  return is_closed_;
}

void NativeWindow::SetBackgroundColor(SkColor background_color) {
  background_color_ = background_color;
}

SkColor NativeWindow::GetBackgroundColor() const {
  return background_color_;
}

gfx::Size NativeWindow::GetSize() const {
  return GetBounds().size();
}

void NativeWindow::SetPosition(const gfx::Point& position, bool animate) {
  SetBounds(gfx::Rect(position, GetSize()), animate);
}

gfx::Point NativeWindow::GetPosition() const {
  return GetBounds().origin();
}

void NativeWindow::SetContentSize(const gfx::Size& size, bool animate) {
  SetSize(ContentBoundsToWindowBounds(gfx::Rect(size)).size(), animate);
}

gfx::Size NativeWindow::GetContentSize() const {
  return GetContentBounds().size();
}

void NativeWindow::SetContentBounds(const gfx::Rect& bounds, bool animate) {
  SetBounds(ContentBoundsToWindowBounds(bounds), animate);
}

gfx::Rect NativeWindow::GetContentBounds() const {
  return WindowBoundsToContentBounds(GetBounds());
}

bool NativeWindow::IsNormal() const {
  return !IsMinimized() && !IsMaximized() && !IsFullscreen();
}

void NativeWindow::SetSizeConstraints(
    const SizeConstraints& window_constraints) {
  size_constraints_ = window_constraints;
  content_size_constraints_.reset();
}

SizeConstraints NativeWindow::GetSizeConstraints() const {
  if (size_constraints_) {
    return *size_constraints_;
  }
  if (!content_size_constraints_) {
    return SizeConstraints();
  }
  SizeConstraints constraints;
  if (content_size_constraints_->HasMaximumSize()) {
    gfx::Rect max_bounds = ContentBoundsToWindowBounds(
        gfx::Rect(content_size_constraints_->GetMaximumSize()));
    constraints.set_maximum_size(max_bounds.size());
  }
  if (content_size_constraints_->HasMinimumSize()) {
    gfx::Rect min_bounds = ContentBoundsToWindowBounds(
        gfx::Rect(content_size_constraints_->GetMinimumSize()));
    constraints.set_minimum_size(min_bounds.size());
  }
  return constraints;
}

void NativeWindow::SetContentSizeConstraints(
    const SizeConstraints& size_constraints) {
  content_size_constraints_ = size_constraints;
  size_constraints_.reset();
}

SizeConstraints NativeWindow::GetContentSizeConstraints() const {
  if (content_size_constraints_) {
    return *content_size_constraints_;
  }
  if (!size_constraints_) {
    return SizeConstraints();
  }
  SizeConstraints constraints;
  if (size_constraints_->HasMaximumSize()) {
    gfx::Rect max_bounds = WindowBoundsToContentBounds(
        gfx::Rect(size_constraints_->GetMaximumSize()));
    constraints.set_maximum_size(max_bounds.size());
  }
  if (size_constraints_->HasMinimumSize()) {
    gfx::Rect min_bounds = WindowBoundsToContentBounds(
        gfx::Rect(size_constraints_->GetMinimumSize()));
    constraints.set_minimum_size(min_bounds.size());
  }
  return constraints;
}

void NativeWindow::SetMinimumSize(const gfx::Size& size) {
  SizeConstraints size_constraints = GetSizeConstraints();
  size_constraints.set_minimum_size(size);
  SetSizeConstraints(size_constraints);
}

gfx::Size NativeWindow::GetMinimumSize() const {
  return GetSizeConstraints().GetMinimumSize();
}

void NativeWindow::SetMaximumSize(const gfx::Size& size) {
  SizeConstraints size_constraints = GetSizeConstraints();
  size_constraints.set_maximum_size(size);
  SetSizeConstraints(size_constraints);
}

gfx::Size NativeWindow::GetMaximumSize() const {
  return GetSizeConstraints().GetMaximumSize();
}

gfx::Size NativeWindow::GetContentMinimumSize() const {
  return GetContentSizeConstraints().GetMinimumSize();
}

gfx::Size NativeWindow::GetContentMaximumSize() const {
  const auto size_constraints = GetContentSizeConstraints();
  gfx::Size maximum_size = size_constraints.GetMaximumSize();
  return maximum_size;
}

// TODO(Guo Xi): review sheet related logic on macOS
void NativeWindow::SetSheetOffset(const double offsetX, const double offsetY) {
  sheet_offset_x_ = offsetX;
  sheet_offset_y_ = offsetY;
}

double NativeWindow::GetSheetOffsetX() {
  return sheet_offset_x_;
}

double NativeWindow::GetSheetOffsetY() {
  return sheet_offset_y_;
}

void NativeWindow::SetParentWindow(NativeWindow* parent) {
  parent_ = parent;
}

void NativeWindow::SetAutoHideCursor(bool auto_hide) {}

void NativeWindow::SetVibrancy(const std::string& type, int duration) {
  vibrancy_ = type;
}

std::string NativeWindow::GetVibrancyTypeForTesting() const {
  return vibrancy_;
}

bool NativeWindow::HasVibrancyView() const {
  return false;
}

std::string NativeWindow::GetVisualEffectStateForTesting() const {
  return "followWindow";
}

std::string NativeWindow::GetNativeVisualEffectStateForTesting() const {
  return "none";
}

#if BUILDFLAG(IS_MAC)
std::optional<std::string> NativeWindow::GetTabbingIdentifier() const {
  return std::nullopt;
}
#endif

void NativeWindow::SetTouchBar(
    std::vector<gin_helper::PersistentDictionary> items) {}

void NativeWindow::RefreshTouchBarItem(const std::string& item_id) {}

void NativeWindow::SetEscapeTouchBarItem(
    gin_helper::PersistentDictionary item) {}

void NativeWindow::SetAutoHideMenuBar(bool auto_hide) {}

bool NativeWindow::IsMenuBarAutoHide() {
  return false;
}

bool NativeWindow::IsMenuBarVisible() {
  return true;
}

double NativeWindow::GetAspectRatio() const {
  return aspect_ratio_;
}

gfx::Size NativeWindow::GetAspectRatioExtraSize() {
  return aspect_ratio_extraSize_;
}

void NativeWindow::SetAspectRatio(double aspect_ratio,
                                  const gfx::Size& extra_size) {
  aspect_ratio_ = aspect_ratio;
  aspect_ratio_extraSize_ = extra_size;
}

void NativeWindow::NotifyWindowCloseButtonClicked() {
  // First ask the observers whether we want to close.
  bool prevent_default = false;
  observers_.Notify(&NativeWindowObserver::WillCloseWindow,
                    std::ref(prevent_default));
  if (prevent_default) {
    WindowList::WindowCloseCancelled(this);
    return;
  }

  CloseImmediately();
}

void NativeWindow::SetSize(const gfx::Size& size, bool animate) {
  SetBounds(gfx::Rect(GetPosition(), size), animate);
}

void NativeWindow::NotifyWindowClosed() {
  if (is_closed_) {
    return;
  }

  is_closed_ = true;
  observers_.Notify(&NativeWindowObserver::OnWindowClosed);
  WindowList::RemoveWindow(this);
}

void NativeWindow::NotifyWindowEndSession() {
  observers_.Notify(&NativeWindowObserver::OnWindowEndSession);
}

void NativeWindow::NotifyWindowBlur() {
  observers_.Notify(&NativeWindowObserver::OnWindowBlur);
}

void NativeWindow::NotifyWindowFocus() {
  observers_.Notify(&NativeWindowObserver::OnWindowFocus);
}

void NativeWindow::NotifyWindowIsKeyChanged(bool is_key) {
  observers_.Notify(&NativeWindowObserver::OnWindowIsKeyChanged, is_key);
}

void NativeWindow::NotifyWindowShow() {
  observers_.Notify(&NativeWindowObserver::OnWindowShow);
}

void NativeWindow::NotifyWindowHide() {
  observers_.Notify(&NativeWindowObserver::OnWindowHide);
}

void NativeWindow::NotifyWindowMaximize() {
  observers_.Notify(&NativeWindowObserver::OnWindowMaximize);
}

void NativeWindow::NotifyWindowUnmaximize() {
  observers_.Notify(&NativeWindowObserver::OnWindowUnmaximize);
}

void NativeWindow::NotifyWindowMinimize() {
  observers_.Notify(&NativeWindowObserver::OnWindowMinimize);
}

void NativeWindow::NotifyWindowRestore() {
  observers_.Notify(&NativeWindowObserver::OnWindowRestore);
}

void NativeWindow::NotifyWindowWillResize(const gfx::Rect& new_bounds,
                                          const gfx::ResizeEdge& edge,
                                          bool& prevent_default) {
  observers_.Notify(&NativeWindowObserver::OnWindowWillResize, new_bounds, edge,
                    std::ref(prevent_default));
}

void NativeWindow::NotifyWindowWillMove(const gfx::Rect& new_bounds,
                                        bool& prevent_default) {
  observers_.Notify(&NativeWindowObserver::OnWindowWillMove, new_bounds,
                    std::ref(prevent_default));
}

void NativeWindow::NotifyWindowResize() {
  observers_.Notify(&NativeWindowObserver::OnWindowResize);
}

void NativeWindow::NotifyWindowResized() {
  observers_.Notify(&NativeWindowObserver::OnWindowResized);
}

void NativeWindow::NotifyWindowMove() {
  observers_.Notify(&NativeWindowObserver::OnWindowMove);
}

void NativeWindow::NotifyWindowMoved() {
  observers_.Notify(&NativeWindowObserver::OnWindowMoved);
}

void NativeWindow::NotifyWindowEnterFullScreen() {
  observers_.Notify(&NativeWindowObserver::OnWindowEnterFullScreen);
}

void NativeWindow::NotifyWindowScrollTouchBegin() {
  observers_.Notify(&NativeWindowObserver::OnWindowScrollTouchBegin);
}

void NativeWindow::NotifyWindowScrollTouchEnd() {
  observers_.Notify(&NativeWindowObserver::OnWindowScrollTouchEnd);
}

void NativeWindow::NotifyWindowSwipe(const std::string& direction) {
  observers_.Notify(&NativeWindowObserver::OnWindowSwipe, direction);
}

void NativeWindow::NotifyWindowRotateGesture(float rotation) {
  observers_.Notify(&NativeWindowObserver::OnWindowRotateGesture, rotation);
}

void NativeWindow::NotifyWindowInputEvent(const base::Value::Dict& details) {
  observers_.Notify(&NativeWindowObserver::OnWindowInputEvent, details);
}

void NativeWindow::NotifyWindowSheetBegin() {
  observers_.Notify(&NativeWindowObserver::OnWindowSheetBegin);
}

void NativeWindow::NotifyWindowSheetEnd() {
  observers_.Notify(&NativeWindowObserver::OnWindowSheetEnd);
}

void NativeWindow::NotifyWindowWillEnterFullScreen() {
  observers_.Notify(&NativeWindowObserver::OnWindowWillEnterFullScreen);
}

void NativeWindow::NotifyWindowWillLeaveFullScreen() {
  observers_.Notify(&NativeWindowObserver::OnWindowWillLeaveFullScreen);
}

void NativeWindow::NotifyWindowLeaveFullScreen() {
  observers_.Notify(&NativeWindowObserver::OnWindowLeaveFullScreen);
}

void NativeWindow::NotifyWindowAlwaysOnTopChanged() {
  observers_.Notify(&NativeWindowObserver::OnWindowAlwaysOnTopChanged);
}

void NativeWindow::NotifyWindowExecuteAppCommand(const std::string& command) {
  observers_.Notify(&NativeWindowObserver::OnExecuteAppCommand, command);
}

void NativeWindow::NotifyTouchBarItemInteraction(
    const std::string& item_id,
    const base::Value::Dict& details) {
  observers_.Notify(&NativeWindowObserver::OnTouchBarItemResult, item_id,
                    details);
}

void NativeWindow::NotifyWindowNewWindowForTab() {
  observers_.Notify(&NativeWindowObserver::OnNewWindowForTab);
}

void NativeWindow::NotifyWindowSystemContextMenu(int x,
                                                 int y,
                                                 bool& prevent_default) {
  observers_.Notify(&NativeWindowObserver::OnSystemContextMenu, x, y,
                    std::ref(prevent_default));
}

}  // namespace lynxtron
