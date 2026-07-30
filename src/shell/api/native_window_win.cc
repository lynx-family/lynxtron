// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/api/native_window_win.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "shell/api/api_menu.h"
#include "shell/api/api_native_image.h"
#include "shell/api/dpi_win.h"
#include "shell/app/window_list.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/options_switches.h"
#include "shell/ui/display/win/screen_win.h"
#include "shell/ui/skia/ext/skia_utils_win.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size_f.h"

namespace lynxtron {
namespace {
const LPCWSTR kUniqueTaskBarClassName = L"Shell_TrayWnd";

void FlipWindowStyle(HWND handle, bool on, DWORD flag) {
  DWORD style = ::GetWindowLong(handle, GWL_STYLE);
  if (on) {
    style |= flag;
  } else {
    style &= ~flag;
  }
  ::SetWindowLong(handle, GWL_STYLE, style);
  // Window's frame styles are cached so we need to call SetWindowPos
  // with the SWP_FRAMECHANGED flag to update cache properly.
  ::SetWindowPos(handle, 0, 0, 0, 0, 0,  // ignored
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

gfx::Rect GetWindowBoundsForClientBounds(HWND hwnd,
                                         const gfx::Rect& client_bounds,
                                         UINT dpi) {
  if (!hwnd) {
    return client_bounds;
  }

  RECT rect = client_bounds.ToRECT();
  const auto style = static_cast<DWORD>(::GetWindowLong(hwnd, GWL_STYLE));
  const auto ex_style = static_cast<DWORD>(::GetWindowLong(hwnd, GWL_EXSTYLE));
  // Keep the client/window mapping in physical pixels. Adjusting a DIP rect
  // would round the non-client insets at the wrong stage.
  // AdjustWindowRectExForDpi accounts for a standard single-row window menu,
  // but it cannot calculate the additional height of a menu that wraps to
  // multiple rows. Multi-row native menus are currently unsupported.
  const BOOL has_menu = ::GetMenu(hwnd) != nullptr;
  ::AdjustWindowRectExForDpi(&rect, style, has_menu, ex_style, dpi);
  return gfx::Rect(rect);
}

gfx::Rect GetClientBoundsForWindowBounds(HWND hwnd,
                                         const gfx::Rect& window_bounds,
                                         UINT dpi) {
  if (!hwnd) {
    return window_bounds;
  }

  // Adjusting an empty client rect gives the signed frame origin and total
  // non-client size. Applying the inverse produces the client rect for an
  // arbitrary outer window rect without consulting the window's current size.
  const gfx::Rect frame_bounds =
      GetWindowBoundsForClientBounds(hwnd, gfx::Rect(), dpi);
  return gfx::Rect(window_bounds.x() - frame_bounds.x(),
                   window_bounds.y() - frame_bounds.y(),
                   std::max(0, window_bounds.width() - frame_bounds.width()),
                   std::max(0, window_bounds.height() - frame_bounds.height()));
}

void SetRestoredBounds(HWND hwnd, const gfx::Rect& bounds) {
  WINDOWPLACEMENT placement = {};
  placement.length = sizeof(WINDOWPLACEMENT);
  if (!::GetWindowPlacement(hwnd, &placement)) {
    return;
  }

  MONITORINFO monitor_info = {};
  monitor_info.cbSize = sizeof(MONITORINFO);
  const RECT rect = bounds.ToRECT();
  const HMONITOR monitor = ::MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
  if (!::GetMonitorInfo(monitor, &monitor_info)) {
    return;
  }

  // WINDOWPLACEMENT::rcNormalPosition uses workspace coordinates, whereas the
  // NativeWindow API and |bounds| use screen coordinates. Account for the
  // monitor work-area offset before updating a minimized window's restore rect.
  gfx::Rect placement_bounds(bounds);
  placement_bounds.Offset(
      monitor_info.rcMonitor.left - monitor_info.rcWork.left,
      monitor_info.rcMonitor.top - monitor_info.rcWork.top);
  placement.rcNormalPosition = placement_bounds.ToRECT();
  ::SetWindowPlacement(hwnd, &placement);
}

}  // namespace

NativeWindowWin::NativeWindowWin(const gin_helper::Dictionary& options,
                                 NativeWindow* parent)
    : NativeWindow(options, parent),
      window_(std::make_unique<ui::HWNDMessageHandler>(this)) {
  options.Get(options::kTitle, &title_);

  // Keep the requested focusable state so delegate activation checks and the
  // initial HWND style stay in sync from window creation.
  options.Get(options::kFocusable, &focusable_);

  // Transparent window must not have thick frame.
  options.Get("thickFrame", &thick_frame_);
  if (transparent()) {
    thick_frame_ = false;
    window_->set_is_translucent(true);
  }

  bool fullscreen = false;
  options.Get(options::kFullscreen, &fullscreen);

  const int width = this->width();
  const int height = this->height();
  const bool use_content_size = this->use_content_size();
  gfx::Rect bounds;
  // At creation time only the requested size is known. Let Windows choose the
  // initial origin, and scale the size independently so frame creation cannot
  // introduce an origin-dependent one-pixel size drift.
  bounds.set_size(
      ::lynxtron::DIPToScreenSizeForWindow(nullptr, gfx::Size(width, height)));

  DWORD frame_style =
      WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
  if (frame()) {
    frame_style |= WS_CAPTION;
  }
  if (resizable() && thick_frame_) {
    frame_style |= WS_THICKFRAME;
  }
  window_->set_window_style(frame_style);

  // Save initial window state.
  if (fullscreen) {
    last_window_state_ = ui::SHOW_STATE_FULLSCREEN;
  } else {
    last_window_state_ = ui::SHOW_STATE_NORMAL;
  }

  DWORD ex_style = 0;
  if (window_type() == "toolbar") {
    ex_style |= WS_EX_TOOLWINDOW;
  }
  if (!focusable_) {
    ex_style |= WS_EX_NOACTIVATE;
  }

  window_->set_window_ex_style(ex_style);

  HWND parent_hwnd = parent ? parent->GetNativeWindowHandle() : HWND_DESKTOP;
  window_->Init(parent_hwnd, bounds);

  if (use_content_size) {
    SetContentSize(gfx::Size(width, height), false);
  }

  const bool minimizable = this->minimizable();
  if (is_modal() && NativeWindow::parent() && !minimizable) {
    SetMinimizable(false);
  }

  SetBackgroundColor(NativeWindow::GetBackgroundColor());
}

NativeWindowWin::~NativeWindowWin() = default;

void NativeWindowWin::Close() {
  if (!IsClosable()) {
    WindowList::WindowCloseCancelled(this);
    return;
  }

  window_->Close();
}

void NativeWindowWin::CloseImmediately() {
  window_->CloseNow();
}

void NativeWindowWin::Focus(bool focus) {
  // For hidden window focus() should do nothing.
  if (!IsVisible()) {
    return;
  }

  if (focus && !IsFocusable()) {
    return;
  }

  if (focus) {
    window_->Activate();
  } else {
    window_->Deactivate();
  }
}

bool NativeWindowWin::IsFocused() {
  return window_->IsActive();
}

void NativeWindowWin::Show() {
  RegisterModalParent();

  window_->Show(IsFocusable() ? ui::WindowShowState::SHOW_STATE_NORMAL
                              : ui::WindowShowState::SHOW_STATE_INACTIVE,
                gfx::Rect());
  // Explicitly focus the window when it can be focused.
  if (IsFocusable()) {
    window_->Activate();
  }

  NotifyWindowShow();
}

void NativeWindowWin::ShowInactive() {
  RegisterModalParent();
  window_->Show(ui::SHOW_STATE_INACTIVE, gfx::Rect());
  NotifyWindowShow();
}

void NativeWindowWin::Hide() {
  UnregisterModalParent();

  window_->Hide();

  NotifyWindowHide();

  // TODO(Guo Xi): SetThumbarButtonsAdded
  // When the window is removed from the taskbar via win.hide(),
  // the thumbnail buttons need to be set up again.
  // Ensure that when the window is hidden,
  // the taskbar host is notified that it should re-add them.
  /// taskbar_host_.SetThumbarButtonsAdded(false);
}

bool NativeWindowWin::IsVisible() {
  // Match Electron's public visibility semantics: a minimized HWND retains
  // WS_VISIBLE, but is not considered visible by the window API.
  return window_->IsVisible() && !window_->IsMinimized();
}

bool NativeWindowWin::IsEnabled() {
  return ::IsWindowEnabled(window_->hwnd());
}

void NativeWindowWin::RegisterModalParent() {
  if (!is_modal() || registered_modal_parent_ || !NativeWindow::parent()) {
    return;
  }

  // Store the exact parent whose modal count was incremented. parent() may be
  // changed before this window is hidden or destroyed.
  registered_modal_parent_ = NativeWindow::parent();
  registered_modal_parent_->IncrementChildModals();
}

void NativeWindowWin::UnregisterModalParent() {
  if (!registered_modal_parent_) {
    return;
  }

  registered_modal_parent_->DecrementChildModals();
  registered_modal_parent_ = nullptr;
}

void NativeWindowWin::IncrementChildModals() {
  num_modal_children_++;
  SetEnabledInternal(ShouldBeEnabled());
}

void NativeWindowWin::DecrementChildModals() {
  if (num_modal_children_ > 0) {
    num_modal_children_--;
  }
  SetEnabledInternal(ShouldBeEnabled());
}

void NativeWindowWin::SetEnabled(bool enable) {
  if (enable != is_enabled_) {
    is_enabled_ = enable;
    SetEnabledInternal(ShouldBeEnabled());
  }
}

bool NativeWindowWin::ShouldBeEnabled() {
  return is_enabled_ && (num_modal_children_ == 0);
}

void NativeWindowWin::SetEnabledInternal(bool enable) {
  if (enable && IsEnabled()) {
    return;
  } else if (!enable && !IsEnabled()) {
    return;
  }

  ::EnableWindow(window_->hwnd(), enable);
}

void NativeWindowWin::Unmaximize() {
  if (IsMinimized()) {
    return;
  }

  if (!frame() || !thick_frame_) {
    if (!last_normal_placement_bounds_.IsEmpty()) {
      // The saved placement is already a physical screen rect. Applying it
      // directly avoids a lossy pixel -> DIP -> pixel restore.
      window_->SetBounds(last_normal_placement_bounds_);
    }
    if (last_window_state_ == ui::SHOW_STATE_MAXIMIZED) {
      last_window_state_ = ui::SHOW_STATE_NORMAL;
      ScheduleApplyPendingContentBounds();
      NotifyWindowUnmaximize();
    }
    return;
  }

  if (transparent()) {
    SetBounds(restore_bounds_, false);
    ScheduleApplyPendingContentBounds();
    NotifyWindowUnmaximize();
    return;
  }

  window_->Restore();
}

bool NativeWindowWin::IsMaximized() const {
  return window_->IsMaximized();

  if (transparent() && !IsMinimized()) {
    // If the window is the same dimensions and placement as the
    // display, we consider it maximized.
    auto display = display::Screen::Get()->GetDisplayNearestWindow(
        GetNativeWindowHandle());
    return GetBounds() == display.work_area();
  }
}

void NativeWindowWin::Minimize() {
  if (window_->IsVisible()) {
    window_->Minimize();
  } else {
    RegisterModalParent();
    window_->Show(ui::SHOW_STATE_MINIMIZED, gfx::Rect());
  }
}

void NativeWindowWin::Restore() {
  if (IsMaximized() && transparent()) {
    SetBounds(restore_bounds_, false);
    NotifyWindowRestore();
    // TODO(Guo Xi): check UpdateThickFrame
    // UpdateThickFrame();
    return;
  }

  window_->Restore();
}

bool NativeWindowWin::IsMinimized() const {
  return window_->IsMinimized();
}

void NativeWindowWin::SetFullScreen(bool fullscreen) {
  if (!IsFullScreenable()) {
    return;
  }

  // There is no native fullscreen state on Windows.
  bool leaving_fullscreen = IsFullscreen() && !fullscreen;

  if (fullscreen) {
    last_window_state_ = ui::SHOW_STATE_FULLSCREEN;
    NotifyWindowEnterFullScreen();
  } else {
    last_window_state_ = ui::SHOW_STATE_NORMAL;
    NotifyWindowLeaveFullScreen();
  }

  // TODO(Guo Xi): rounded corners
  // If round corners are enabled,
  // they need to be set based on whether the window is fullscreen.
  // if (rounded_corner_) {
  //   SetRoundedCorners(!fullscreen);
  // }

  // TODO(Guo Xi): thick frame
  // For window without WS_THICKFRAME style, we can not call SetFullscreen().
  // This path will be used for transparent windows as well.
  // if (!thick_frame_) {
  //  if (fullscreen) {
  //      // TODO(Guo Xi): add display
  //    restore_bounds_ = GetBounds();
  //    auto display =
  //        display::Screen::GetScreen()->GetDisplayNearestPoint(GetPosition());
  //    SetBounds(display.bounds(), false);
  //  } else {
  //    SetBounds(restore_bounds_, false);
  //  }
  //  return;
  //}

  // We set the new value after notifying, so we can handle the size event
  // correctly.
  window_->SetFullscreen(fullscreen);

  // If restoring from fullscreen and the window isn't visible, force visible,
  // else a non-responsive window shell could be rendered.
  // (this situation may arise when app starts with fullscreen: true)
  // Note: the following must be after "widget()->SetFullscreen(fullscreen);"
  if (leaving_fullscreen && !IsVisible()) {
    FlipWindowStyle(window_->hwnd(), true, WS_VISIBLE);
  }

  if (leaving_fullscreen) {
    ScheduleApplyPendingContentBounds();
  }

  // TODO(Guo Xi): Here is the menubar-related code, with reference to Electron.
}

bool NativeWindowWin::IsFullscreen() const {
  return window_->IsFullscreen();
}

void NativeWindowWin::SetBounds(const gfx::Rect& bounds, bool animate) {
  static_cast<void>(animate);
  UpdateBounds(bounds.origin(), bounds.size());
}

void NativeWindowWin::SetSize(const gfx::Size& size, bool animate) {
  static_cast<void>(animate);
  UpdateBounds(std::nullopt, size);
}

void NativeWindowWin::SetPosition(const gfx::Point& position, bool animate) {
  static_cast<void>(animate);
  UpdateBounds(position, std::nullopt);
}

void NativeWindowWin::UpdateBounds(const std::optional<gfx::Point>& dip_origin,
                                   const std::optional<gfx::Size>& dip_size) {
  DCHECK(dip_origin || dip_size);

  if (dip_size && !CanResize()) {
    // Programmatic resizes of a fixed window must also update its native
    // tracking limits. Keep those limits in window-space DIP, matching
    // Electron's non-resizable constraint semantics.
    SetFixedWindowSizeConstraints(*dip_size);
  }

  // Read the current rect in pixels so a position-only or size-only API call
  // can retain the untouched physical field exactly.
  const gfx::Rect current_pixel_bounds =
      IsMinimized() ? window_->GetRestoredBounds()
                    : window_->GetWindowBoundsInScreen();
  const gfx::Rect pixel_bounds =
      UpdateScreenRectForWindow(current_pixel_bounds, dip_origin, dip_size);
  if (IsMinimized()) {
    // SetWindowPos cannot reliably change a minimized window's normal bounds.
    // Update WINDOWPLACEMENT so the requested bounds take effect on restore.
    SetRestoredBounds(GetNativeWindowHandle(), pixel_bounds);
  } else {
    window_->SetBounds(pixel_bounds);
  }
}

void NativeWindowWin::SetFixedWindowSizeConstraints(
    const gfx::Size& window_size) {
  // Both limits are replaced, so write the authoritative window constraints
  // atomically. Calling SetMinimumSize() and SetMaximumSize() separately would
  // first convert any old content constraints even though both values are
  // immediately discarded.
  SetSizeConstraints(SizeConstraints(window_size, window_size));
}

gfx::Rect NativeWindowWin::GetBounds() const {
  if (IsMinimized()) {
    return ScreenToDIPRectForWindow(nullptr, window_->GetRestoredBounds());
  }

  gfx::Rect bounds = window_->GetWindowBoundsInScreen();
  return ScreenToDIPRectForWindow(GetNativeWindowHandle(), bounds);
}

void NativeWindowWin::SetContentSize(const gfx::Size& size, bool animate) {
  static_cast<void>(animate);

  // Enforce content constraints in DIP before any pixel rounding so the
  // requested size and its minimum/maximum limits share one coordinate space.
  const gfx::Size clamped_size = GetContentSizeConstraints().ClampSize(size);
  if (!IsNormal()) {
    // Windows may overwrite bounds while completing a minimize, maximize, or
    // fullscreen transition. Keep only the latest content request and apply it
    // after the window has returned to the normal state.
    pending_content_bounds_.reset();
    pending_content_size_ = clamped_size;
    return;
  }

  pending_content_size_.reset();
  pending_content_bounds_.reset();
  // Content geometry is expressed in DIP, but the non-client frame mapping is
  // a Win32 operation in physical pixels. Convert once at that boundary.
  gfx::Rect pixel_client_bounds = window_->GetClientAreaBoundsInScreen();
  pixel_client_bounds.set_size(::lynxtron::DIPToScreenSizeForWindow(
      GetNativeWindowHandle(), clamped_size));
  gfx::Rect pixel_window_bounds = pixel_client_bounds;
  if (frame()) {
    pixel_window_bounds = GetWindowBoundsForClientBounds(
        GetNativeWindowHandle(), pixel_client_bounds,
        display::win::ScreenWin::GetDPIForHWND(GetNativeWindowHandle()));
  }
  if (!CanResize()) {
    const gfx::Size window_size = ScreenToDIPSizeForWindow(
        GetNativeWindowHandle(), pixel_window_bounds.size());
    SetFixedWindowSizeConstraints(window_size);
  }
  window_->SetBounds(pixel_window_bounds);
}

void NativeWindowWin::SetContentBounds(const gfx::Rect& bounds, bool animate) {
  static_cast<void>(animate);

  if (!IsNormal()) {
    // SetContentSize() and SetContentBounds() share one pending request. The
    // most recently called API wins while the window is not in the normal
    // state.
    pending_content_size_.reset();
    pending_content_bounds_ = bounds;
    return;
  }

  pending_content_bounds_.reset();
  pending_content_size_.reset();
  // A target screen rect must select its display from the target bounds, not
  // from the HWND's current monitor; the window may be moving across DPIs.
  const gfx::Rect pixel_client_bounds =
      DIPToScreenRectForWindow(nullptr, bounds);
  gfx::Rect pixel_window_bounds = pixel_client_bounds;
  if (frame()) {
    pixel_window_bounds = GetWindowBoundsForClientBounds(
        GetNativeWindowHandle(), pixel_client_bounds,
        display::win::ScreenWin::GetDPIForScreenRect(pixel_client_bounds));
  }
  if (!CanResize()) {
    const gfx::Size window_size =
        ScreenToDIPRectForWindow(nullptr, pixel_window_bounds).size();
    SetFixedWindowSizeConstraints(window_size);
  }
  window_->SetBounds(pixel_window_bounds);
}

void NativeWindowWin::ApplyPendingContentBounds() {
  // During a minimized-to-maximized transition the HWND can momentarily report
  // restored before the maximize notification arrives. Use the tracked target
  // state so a pending request is not consumed during that gap.
  if (last_window_state_ != ui::SHOW_STATE_NORMAL) {
    return;
  }

  if (pending_content_bounds_) {
    const gfx::Rect bounds = *pending_content_bounds_;
    SetContentBounds(bounds, false);
  } else if (pending_content_size_) {
    const gfx::Size size = *pending_content_size_;
    SetContentSize(size, false);
  }
}

void NativeWindowWin::ScheduleApplyPendingContentBounds() {
  if (!pending_content_bounds_ && !pending_content_size_) {
    return;
  }

  // Do not change the HWND bounds from inside a restore/maximize state
  // notification. Windows may still apply its saved window placement after the
  // notification returns and overwrite a synchronous SetBounds(). Posting to
  // the current UI thread lets the native state transition finish first.
  //
  // The window can be destroyed before the task runs, so bind through a
  // WeakPtr instead of retaining a raw NativeWindowWin pointer.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&NativeWindowWin::ApplyPendingContentBounds,
                                weak_factory_.GetWeakPtr()));
}

gfx::Rect NativeWindowWin::GetContentBounds() const {
  return ScreenToDIPRectForWindow(GetNativeWindowHandle(),
                                  window_->GetClientAreaBoundsInScreen());
}

float NativeWindowWin::GetDevicePixelRatio() const {
  return display::win::ScreenWin::GetScaleFactorForHWND(
      GetNativeWindowHandle());
}

gfx::Rect NativeWindowWin::GetNormalBounds() const {
  if (IsMaximized() && transparent()) {
    return restore_bounds_;
  }
  return ScreenToDIPRectForWindow(nullptr, window_->GetRestoredBounds());
}

void NativeWindowWin::SetResizable(bool resizable) {
  if (resizable != this->resizable()) {
    set_resizable(resizable);
    if (resizable) {
      if (old_size_constraints_.space == ConstraintSpace::kContent) {
        SetContentSizeConstraints(old_size_constraints_.constraints);
      } else {
        SetSizeConstraints(old_size_constraints_.constraints);
      }
    } else {
      if (content_size_constraints_) {
        old_size_constraints_ = {ConstraintSpace::kContent,
                                 *content_size_constraints_};
      } else {
        old_size_constraints_ = {ConstraintSpace::kWindow,
                                 GetSizeConstraints()};
      }
      const gfx::Size window_size = GetSize();
      SetSizeConstraints(SizeConstraints(window_size, window_size));
    }
    window_->SizeConstraintsChanged();
  }
}

void NativeWindowWin::MoveTop() {
  window_->StackAtTop();
}

bool NativeWindowWin::IsResizable() const {
  if (frame()) {
    return ::GetWindowLong(GetNativeWindowHandle(), GWL_STYLE) & WS_THICKFRAME;
  }

  return resizable();
}

void NativeWindowWin::SetAspectRatio(double aspect_ratio,
                                     const gfx::Size& extra_size) {
  NativeWindow::SetAspectRatio(aspect_ratio, extra_size);
  gfx::SizeF aspect(aspect_ratio, 1.0);
  // Scale up because SetAspectRatio() truncates aspect value to int
  aspect.Scale(100);

  window_->SetAspectRatio(aspect.width() / aspect.height());
}

void NativeWindowWin::SetMovable(bool movable) {
  movable_ = movable;
}

bool NativeWindowWin::IsMovable() const {
  return movable_;
}

void NativeWindowWin::SetMinimizable(bool minimizable) {
  FlipWindowStyle(GetNativeWindowHandle(), minimizable, WS_MINIMIZEBOX);
  set_minimizable(minimizable);
}

bool NativeWindowWin::IsMinimizable() const {
  return ::GetWindowLong(GetNativeWindowHandle(), GWL_STYLE) & WS_MINIMIZEBOX;
}

void NativeWindowWin::SetMaximizable(bool maximizable) {
  FlipWindowStyle(GetNativeWindowHandle(), maximizable, WS_MAXIMIZEBOX);
  set_maximizable(maximizable);
}

bool NativeWindowWin::IsMaximizable() const {
  return ::GetWindowLong(GetNativeWindowHandle(), GWL_STYLE) & WS_MAXIMIZEBOX;
}

bool NativeWindowWin::IsExcludedFromShownWindowsMenu() {
  // return false on unsupported platforms
  return false;
}

void NativeWindowWin::SetFullScreenable(bool fullscreenable) {
  fullscreenable_ = fullscreenable;
}

bool NativeWindowWin::IsFullScreenable() const {
  return fullscreenable_;
}

void NativeWindowWin::SetClosable(bool closable) {
  HMENU menu = GetSystemMenu(GetNativeWindowHandle(), false);
  if (closable) {
    EnableMenuItem(menu, SC_CLOSE, MF_BYCOMMAND | MF_ENABLED);
  } else {
    EnableMenuItem(menu, SC_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
  }
  set_closable(closable);
}

bool NativeWindowWin::IsClosable() const {
  HMENU menu = GetSystemMenu(GetNativeWindowHandle(), false);
  MENUITEMINFO info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  info.fMask = MIIM_STATE;
  if (!GetMenuItemInfo(menu, SC_CLOSE, false, &info)) {
    return false;
  }
  return !(info.fState & MFS_DISABLED);
}

void NativeWindowWin::SetAlwaysOnTop(ui::ZOrderLevel z_order,
                                     const std::string& level,
                                     int relativeLevel) {
  bool level_changed = z_order != z_order_;
  z_order_ = z_order;

  bool on_top = z_order != ui::ZOrderLevel::kNormal;
  window_->SetAlwaysOnTop(on_top);

  // TODO(Guo Xi): task bar
  // Reset the placement flag.
  behind_task_bar_ = false;
  if (z_order != ui::ZOrderLevel::kNormal) {
    // On macOS the window is placed behind the Dock for the following levels.
    // Re-use the same names on Windows to make it easier for the user.
    static const std::vector<std::string> levels = {
        "floating", "torn-off-menu", "modal-panel", "main-menu", "status"};
    behind_task_bar_ = base::Contains(levels, level);
  }
  MoveBehindTaskBarIfNeeded();

  // This must be notified at the very end or IsAlwaysOnTop
  // will not yet have been updated to reflect the new status
  if (level_changed) {
    NativeWindow::NotifyWindowAlwaysOnTopChanged();
  }
}

ui::ZOrderLevel NativeWindowWin::GetZOrderLevel() const {
  return z_order_;
}

void NativeWindowWin::Center() {
  // Center the logical window bounds in the display work area. Keeping origin
  // and size conversions separate avoids asymmetric frame inset and
  // fractional-scale DIP/pixel round-trip errors. GetDisplayNearestWindow
  // cannot be used because ScreenWin's NativeWindow-to-HWND conversion is not
  // implemented, so select the display matching the logical bounds instead.
  const gfx::Rect current_bounds = GetBounds();
  auto display = display::Screen::Get()->GetDisplayMatching(current_bounds);
  gfx::Rect bounds = display.work_area();
  bounds.ClampToCenteredSize(current_bounds.size());
  if (!IsNormal()) {
    SetBounds(bounds, false);
    return;
  }

  // For a normal window, move only the physical origin and retain the current
  // pixel size. Rebuilding the full rect from DIP could change its size by one
  // pixel at fractional scale factors.
  const gfx::Point pixel_origin =
      DIPToScreenRectForWindow(nullptr, gfx::Rect(bounds.origin(), gfx::Size()))
          .origin();
  gfx::Rect pixel_bounds = window_->GetWindowBoundsInScreen();
  pixel_bounds.set_origin(pixel_origin);
  window_->SetBounds(pixel_bounds);
}

void NativeWindowWin::SetTitle(const std::string& title) {
  title_ = title;
  std::u16string window_title = base::UTF8ToUTF16(title);
  base::i18n::AdjustStringForLocaleDirection(&window_title);
  window_->SetTitle(window_title);
}

std::string NativeWindowWin::GetTitle() const {
  return title_;
}

void NativeWindowWin::FlashFrame(bool flash) {
  // The Chromium's implementation has a bug stopping flash.
  if (!flash) {
    FLASHWINFO fwi;
    fwi.cbSize = sizeof(fwi);
    fwi.hwnd = GetNativeWindowHandle();
    fwi.dwFlags = FLASHW_STOP;
    fwi.uCount = 0;
    FlashWindowEx(&fwi);
    return;
  }
}

void NativeWindowWin::SetSkipTaskbar(bool skip) {
  Microsoft::WRL::ComPtr<ITaskbarList> taskbar;
  if (FAILED(::CoCreateInstance(CLSID_TaskbarList, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&taskbar))) ||
      FAILED(taskbar->HrInit())) {
    return;
  }
  if (skip) {
    taskbar->DeleteTab(window_->hwnd());
  } else {
    taskbar->AddTab(window_->hwnd());
    // TODO(Guo Xi): Add description here
    // taskbar_host_.RestoreThumbarButtons(GetAcceleratedWidget());
  }
}

void NativeWindowWin::SetSimpleFullScreen(bool simple_fullscreen) {
  SetFullScreen(simple_fullscreen);
}

bool NativeWindowWin::IsSimpleFullScreen() {
  return IsFullscreen();
}

void NativeWindowWin::SetBackgroundColor(SkColor background_color) {
  NativeWindow::SetBackgroundColor(background_color);

  HWND hwnd = GetNativeWindowHandle();
  if (!hwnd) {
    return;
  }

  background_brush_ = base::win::ScopedGDIObject<HBRUSH>(
      ::CreateSolidBrush(skia::SkColorToCOLORREF(background_color)));
  SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND,
                  reinterpret_cast<LONG_PTR>(background_brush_.get()));

  ::RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

SkColor NativeWindowWin::GetBackgroundColor() const {
  return NativeWindow::GetBackgroundColor();
}

void NativeWindowWin::SetHasShadow(bool has_shadow) {
  // wm::SetShadowElevation(GetNativeWindow(),
  //                       has_shadow ? wm::kShadowElevationInactiveWindow
  //                                  : wm::kShadowElevationNone);
}

bool NativeWindowWin::HasShadow() {
  // return GetNativeWindow()->GetProperty(wm::kShadowElevationKey) !=
  //       wm::kShadowElevationNone;
  return true;
}

void NativeWindowWin::SetOpacity(const double opacity) {
  const double boundedOpacity = std::clamp(opacity, 0.0, 1.0);
  HWND hwnd = GetNativeWindowHandle();
  if (!layered_) {
    LONG ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
    ex_style |= WS_EX_LAYERED;
    ::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);
    layered_ = true;
  }
  ::SetLayeredWindowAttributes(hwnd, 0, boundedOpacity * 255, LWA_ALPHA);
  opacity_ = boundedOpacity;
}

double NativeWindowWin::GetOpacity() {
  return opacity_;
}

void NativeWindowWin::SetFocusable(bool focusable) {
  focusable_ = focusable;
  UpdateFocusableStyle();
  SetSkipTaskbar(!focusable);
  if (!focusable) {
    Focus(false);
  }
}

void NativeWindowWin::UpdateFocusableStyle() {
  LONG ex_style = ::GetWindowLong(GetNativeWindowHandle(), GWL_EXSTYLE);
  if (focusable_) {
    ex_style &= ~WS_EX_NOACTIVATE;
  } else {
    ex_style |= WS_EX_NOACTIVATE;
  }
  ::SetWindowLong(GetNativeWindowHandle(), GWL_EXSTYLE, ex_style);
}

bool NativeWindowWin::IsFocusable() const {
  LONG ex_style = ::GetWindowLong(GetNativeWindowHandle(), GWL_EXSTYLE);
  bool no_activate = ex_style & WS_EX_NOACTIVATE;
  return !no_activate && focusable_;
}

void NativeWindowWin::SetParentWindow(NativeWindow* parent) {
  NativeWindow* const old_parent = NativeWindow::parent();
  const bool was_registered = registered_modal_parent_ != nullptr;
  // Transfer an active modal registration from the old parent to the new one.
  // Checking parent() alone is insufficient because Hide() may already have
  // removed this window from its parent's modal-child count.
  if (was_registered && old_parent != parent) {
    UnregisterModalParent();
  }
  NativeWindow::SetParentWindow(parent);
  if (was_registered && old_parent != parent) {
    RegisterModalParent();
  }
  HWND parent_hwnd = parent ? parent->GetNativeWindowHandle() : nullptr;
  ::SetWindowLongPtr(window_->hwnd(), GWLP_HWNDPARENT,
                     reinterpret_cast<LONG_PTR>(parent_hwnd));
}

void NativeWindowWin::SetVisibleOnAllWorkspaces(bool visible,
                                                bool visibleonfullscreen,
                                                bool skiptransformprocesstype) {
}

bool NativeWindowWin::IsVisibleOnAllWorkspaces() {
  return false;
}

NativeWindowHandle NativeWindowWin::GetNativeWindowHandle() const {
  return window_->hwnd();
}

void NativeWindowWin::SetIcon(api::NativeImage* icon) {
  if (!icon) {
    return;
  }
  const int small_icon_size = GetSystemMetrics(SM_CXSMICON);
  const int large_icon_size = GetSystemMetrics(SM_CXICON);
  HICON small_icon = icon->GetHICON(small_icon_size);
  HICON large_icon = icon->GetHICON(large_icon_size);
  if (!small_icon && !large_icon) {
    return;
  }
  if (!small_icon) {
    small_icon = large_icon;
  }
  if (!large_icon) {
    large_icon = small_icon;
  }
  SetIcon(small_icon, large_icon);
}

void NativeWindowWin::SetIcon(HICON window_icon, HICON app_icon) {
  // We are responsible for storing the images.
  window_icon_ = base::win::ScopedGDIObject<HICON>(CopyIcon(window_icon));
  app_icon_ = base::win::ScopedGDIObject<HICON>(CopyIcon(app_icon));

  HWND hwnd = GetNativeWindowHandle();
  SendMessage(hwnd, WM_SETICON, ICON_SMALL,
              reinterpret_cast<LPARAM>(window_icon_.get()));
  SendMessage(hwnd, WM_SETICON, ICON_BIG,
              reinterpret_cast<LPARAM>(app_icon_.get()));
}

void NativeWindowWin::MoveBehindTaskBarIfNeeded() {
  if (behind_task_bar_) {
    const HWND task_bar_hwnd = ::FindWindow(kUniqueTaskBarClassName, nullptr);
    ::SetWindowPos(GetNativeWindowHandle(), task_bar_hwnd, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
}

ui::WindowShowState NativeWindowWin::GetRestoredState() {
  if (IsMaximized()) {
    // Only restore Maximized state when window is NOT transparent style
    if (!transparent()) {
      return ui::SHOW_STATE_MAXIMIZED;
    }
  }

  if (IsFullscreen()) {
    return ui::SHOW_STATE_FULLSCREEN;
  }

  return ui::SHOW_STATE_NORMAL;
}

gfx::Rect NativeWindowWin::ContentBoundsToWindowBounds(
    const gfx::Rect& bounds) const {
  if (!frame()) {
    return bounds;
  }

  HWND hwnd = GetNativeWindowHandle();
  // The public mapping is DIP -> DIP, but Win32 defines non-client metrics in
  // pixels. Convert at the boundary, apply the frame, then convert back.
  const gfx::Rect pixel_client_bounds = DIPToScreenRectForWindow(hwnd, bounds);
  return ScreenToDIPRectForWindow(
      hwnd, GetWindowBoundsForClientBounds(
                hwnd, pixel_client_bounds,
                display::win::ScreenWin::GetDPIForHWND(hwnd)));
}

gfx::Rect NativeWindowWin::WindowBoundsToContentBounds(
    const gfx::Rect& bounds) const {
  if (!frame()) {
    return bounds;
  }

  HWND hwnd = GetNativeWindowHandle();
  // This is the exact inverse of ContentBoundsToWindowBounds for the supported
  // system frame and single-row menu model.
  const gfx::Rect pixel_window_bounds = DIPToScreenRectForWindow(hwnd, bounds);
  return ScreenToDIPRectForWindow(
      hwnd, GetClientBoundsForWindowBounds(
                hwnd, pixel_window_bounds,
                display::win::ScreenWin::GetDPIForHWND(hwnd)));
}

////////////////////////////////////////////////////////////////////////////////
// HWNDMessageHandlerDelegate implementation:

ui::FrameMode NativeWindowWin::GetFrameMode() const {
  return frame() ? ui::FrameMode::SYSTEM_DRAWN : ui::FrameMode::CUSTOM_DRAWN;
}

bool NativeWindowWin::HasFrame() const {
  return NativeWindow::frame();
}

bool NativeWindowWin::ShouldPaintAsActive() const {
  return false;
}

bool NativeWindowWin::CanResize() const {
  return resizable() && thick_frame_;
}

bool NativeWindowWin::CanMaximize() const {
  return maximizable_;
}

bool NativeWindowWin::CanMinimize() const {
  return minimizable_;
}

bool NativeWindowWin::CanActivate() const {
  return focusable_;
}

bool NativeWindowWin::WantsMouseEventsWhenInactive() const {
  return false;
}

bool NativeWindowWin::IsModal() const {
  return is_modal();
}

int NativeWindowWin::GetInitialShowState() const {
  return ui::WindowShowState::SHOW_STATE_NORMAL;
}

bool NativeWindowWin::GetClientAreaInsets(gfx::Insets* insets,
                                          HMONITOR monitor) const {
  return false;
}

bool NativeWindowWin::GetDwmFrameInsetsInPixels(gfx::Insets* insets) const {
  return false;
}

void NativeWindowWin::GetMinMaxSize(gfx::Size* min_size,
                                    gfx::Size* max_size) const {
  // Return the authoritative constraint values without converting content DIP
  // to window DIP. HWNDMessageHandler performs the single DIP -> pixel
  // conversion and adds the non-client frame in pixels at the Win32 boundary.
  const SizeConstraints constraints = content_size_constraints_
                                          ? *content_size_constraints_
                                          : GetSizeConstraints();
  *min_size = constraints.GetMinimumSize();
  *max_size = constraints.GetMaximumSize();
  // NativeWindow uses zero for an unspecified maximum. Convert each dimension
  // independently to INT_MAX so a one-axis maximum keeps the other unbounded
  // through DPI scaling and non-client frame expansion.
  if (!max_size->width()) {
    max_size->set_width(INT_MAX);
  }
  if (!max_size->height()) {
    max_size->set_height(INT_MAX);
  }
}

bool NativeWindowWin::MinMaxSizeIsClientSize() const {
  // The unit is always DIP; this flag describes whether those DIP values bound
  // the client area or the outer window.
  return content_size_constraints_.has_value();
}

gfx::Size NativeWindowWin::GetRootViewSize() const {
  return gfx::Size();
}

gfx::Size NativeWindowWin::DIPToScreenSize(const gfx::Size& dip_size) const {
  return lynxtron::DIPToScreenSizeForWindow(window_->hwnd(), dip_size);
}

void NativeWindowWin::ResetWindowControls() {}

void NativeWindowWin::HandleActivationChanged(bool active) {
  if (active) {
    NotifyWindowFocus();
  } else {
    NotifyWindowBlur();
  }
}

bool NativeWindowWin::HandleAppCommand(int command) {
  return false;
}

void NativeWindowWin::HandleCancelMode() {}

void NativeWindowWin::HandleClose() {
  Close();
}

bool NativeWindowWin::HandleCommand(int command) {
  return api::Menu::ExecuteCommandFromApplicationMenu(command, 0);
}

void NativeWindowWin::HandleCreate() {}

void NativeWindowWin::HandleDestroying() {}

void NativeWindowWin::HandleDisplayChange() {}

void NativeWindowWin::HandleBeginWMSizeMove() {
  set_is_in_size_move(true);
}

void NativeWindowWin::HandleEndWMSizeMove() {
  set_is_in_size_move(false);
  NotifyWindowResized();
}

void NativeWindowWin::HandleWorkAreaChanged() {}

void NativeWindowWin::HandleVisibilityChanged(bool visible) {}

void NativeWindowWin::HandleWindowMinimizedOrRestored(bool restored) {
  if (restored) {
    if (last_window_state_ != ui::SHOW_STATE_MINIMIZED) {
      return;
    }
    last_window_state_ = restored_window_state_;
    ScheduleApplyPendingContentBounds();
    NotifyWindowRestore();
    return;
  }

  if (last_window_state_ == ui::SHOW_STATE_MINIMIZED) {
    return;
  }

  last_normal_placement_bounds_ = window_->GetRestoredBounds();

  restored_window_state_ = last_window_state_;
  last_window_state_ = ui::SHOW_STATE_MINIMIZED;
  NotifyWindowMinimize();
}

void NativeWindowWin::HandleWindowMaximized(bool maximized) {
  if (maximized) {
    if (last_window_state_ == ui::SHOW_STATE_MAXIMIZED) {
      return;
    }

    last_normal_placement_bounds_ = window_->GetRestoredBounds();

    last_window_state_ = ui::SHOW_STATE_MAXIMIZED;
    NotifyWindowMaximize();
    return;
  }

  if (last_window_state_ != ui::SHOW_STATE_MAXIMIZED) {
    return;
  }
  if (IsMinimized()) {
    return;
  }

  last_window_state_ = ui::SHOW_STATE_NORMAL;
  ScheduleApplyPendingContentBounds();
  NotifyWindowUnmaximize();
}

void NativeWindowWin::HandleFrameChanged() {}

void NativeWindowWin::HandleNativeFocus(HWND last_focused_window) {}

void NativeWindowWin::HandleNativeBlur(HWND focused_window) {}

void NativeWindowWin::HandleMenuLoop(bool in_menu_loop) {}

void NativeWindowWin::HandleWindowSizeChanging() {}

void NativeWindowWin::HandleWindowSizeUnchanged() {}

void NativeWindowWin::HandleDestroyed() {
  NativeWindow* const modal_parent = registered_modal_parent_;
  UnregisterModalParent();
  if (modal_parent) {
    modal_parent->Focus(true);
  }
  NotifyWindowClosed();
}

bool NativeWindowWin::HandleInitialFocus(ui::WindowShowState show_state) {
  // Need to focus on LynxView window handle, so LynxView can get keyboard
  // message
  return focusable_;
}

void NativeWindowWin::HandleMove() {
  NotifyWindowMove();
}

void NativeWindowWin::HandleMoved() {
  NotifyWindowMoved();
}

bool NativeWindowWin::HandleMoving(RECT* rect) {
  is_moving_ = true;
  bool prevent_default = false;
  gfx::Rect bounds = gfx::Rect(*rect);
  HWND hwnd = GetNativeWindowHandle();
  gfx::Rect dpi_bounds = ScreenToDIPRectForWindow(nullptr, bounds);
  NotifyWindowWillMove(dpi_bounds, prevent_default);
  if (!movable_ || prevent_default) {
    ::GetWindowRect(hwnd, rect);
    return true;
  }
  return false;
}

void NativeWindowWin::HandleClientSizeChanged(const gfx::Size& new_size) {
  NotifyWindowResize();
}

void NativeWindowWin::Maximize() {
  if (!transparent()) {
    if (IsVisible()) {
      window_->Maximize();
    } else {
      RegisterModalParent();
      window_->Show(ui::SHOW_STATE_MAXIMIZED, gfx::Rect());
      NotifyWindowShow();
    }
  } else {
    restore_bounds_ = GetBounds();
    auto display = display::Screen::Get()->GetDisplayNearestWindow(
        GetNativeWindowHandle());
    SetBounds(display.work_area(), false);
  }
}

bool NativeWindowWin::PreHandleMSG(UINT message,
                                   WPARAM w_param,
                                   LPARAM l_param,
                                   LRESULT* result) {
  NotifyWindowMessage(message, w_param, l_param);

  return false;
}

void NativeWindowWin::PostHandleMSG(UINT message,
                                    WPARAM w_param,
                                    LPARAM l_param) {}

void NativeWindowWin::HandleWindowScaleFactorChanged(
    float window_scale_factor) {}

// static
NativeWindow* NativeWindow::Create(const gin_helper::Dictionary& options,
                                   NativeWindow* parent) {
  return new NativeWindowWin(options, parent);
}

void NativeWindowWin::SetProgressBar(double progress,
                                     const ProgressState state) {
  if (!taskbar_list_) {
    HRESULT hr =
        ::CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&taskbar_list_));
    if (FAILED(hr)) {
      return;
    }
  }

  HWND hwnd = GetNativeWindowHandle();
  if (state == ProgressState::kNone) {
    taskbar_list_->SetProgressState(hwnd, TBPF_NOPROGRESS);
    return;
  }

  TBPFLAG flag = TBPF_NORMAL;
  switch (state) {
    case ProgressState::kIndeterminate:
      flag = TBPF_INDETERMINATE;
      break;
    case ProgressState::kError:
      flag = TBPF_ERROR;
      break;
    case ProgressState::kPaused:
      flag = TBPF_PAUSED;
      break;
    case ProgressState::kNormal:
      flag = TBPF_NORMAL;
      break;
    default:
      flag = TBPF_NORMAL;
      break;
  }

  taskbar_list_->SetProgressState(hwnd, flag);
  if (state != ProgressState::kIndeterminate) {
    taskbar_list_->SetProgressValue(
        hwnd, static_cast<ULONGLONG>(progress * 100), 100);
  }
}

void NativeWindowWin::NotifyWindowMessage(UINT message,
                                          WPARAM w_param,
                                          LPARAM l_param) {
  observers_.Notify(&NativeWindowObserver::OnWindowMessage, message, w_param,
                    l_param);
}

}  // namespace lynxtron
