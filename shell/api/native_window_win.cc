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
#include "shell/api/api_native_image.h"
#include "shell/api/dpi_win.h"
#include "shell/app/window_list.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/options_switches.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size_f.h"

namespace lynxtron {
namespace {
const LPCWSTR kUniqueTaskBarClassName = L"Shell_TrayWnd";
const float kDefaultDPI = 96.f;

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
                                         const gfx::Rect& client_bounds) {
  if (!hwnd) {
    return client_bounds;
  }
  RECT rect = client_bounds.ToRECT();
  auto style = static_cast<DWORD>(::GetWindowLong(hwnd, GWL_STYLE));
  auto ex_style = static_cast<DWORD>(::GetWindowLong(hwnd, GWL_EXSTYLE));
  ::AdjustWindowRectEx(&rect, style, FALSE, ex_style);
  return gfx::Rect(rect);
}

}  // namespace

NativeWindowWin::NativeWindowWin(const gin_helper::Dictionary& options,
                                 NativeWindow* parent)
    : NativeWindow(options, parent),
      window_(std::make_unique<ui::HWNDMessageHandler>(this)) {
  options.Get(options::kTitle, &title_);

  // On Windows we rely on the CanResize() to indicate whether window can be
  // resized, and it should be set before window is created.
  options.Get(options::kFocusable, &can_activate_);

  // Transparent window must not have thick frame.
  options.Get("thickFrame", &thick_frame_);
  if (transparent()) {
    thick_frame_ = false;
    window_->set_is_translucent(true);
  }

  bool fullscreen = false;
  options.Get(options::kFullscreen, &fullscreen);

  int x = -1, y = -1;
  options.Get(options::kX, &x);
  options.Get(options::kY, &y);

  const int width = this->width();
  const int height = this->height();
  const bool use_content_size = this->use_content_size();
  gfx::Rect bounds(x, y, width, height);
  bounds = DIPToScreenRect(GetNativeWindowHandle(), bounds);

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

  window_->set_window_ex_style(ex_style);

  HWND parent_hwnd = parent ? parent->GetNativeWindowHandle() : HWND_DESKTOP;
  window_->Init(parent_hwnd, bounds);

  if (use_content_size) {
    SetContentSize(gfx::Size(width, height), false);
  }

  const bool minimizable = options.ValueOrDefault(options::kMinimizable, true);
  if (is_modal() && NativeWindow::parent() && !minimizable) {
    SetMinimizable(false);
  }
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
  const bool was_visible = IsVisible();
  if (!was_visible && is_modal() && NativeWindow::parent()) {
    parent()->IncrementChildModals();
  }

  window_->Show(ui::WindowShowState::SHOW_STATE_NORMAL, gfx::Rect());
  //// explicitly focus the window
  window_->Activate();

  NotifyWindowShow();
}

void NativeWindowWin::ShowInactive() {
  const bool was_visible = IsVisible();
  if (!was_visible && is_modal() && NativeWindow::parent()) {
    parent()->IncrementChildModals();
  }
  window_->Show(ui::SHOW_STATE_INACTIVE, gfx::Rect());
  NotifyWindowShow();
}

void NativeWindowWin::Hide() {
  if (is_modal() && NativeWindow::parent()) {
    // static_cast<NativeWindowWin*>(parent())->DecrementChildModals();
    parent()->DecrementChildModals();
  }

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
  return window_->IsVisible();
}

bool NativeWindowWin::IsEnabled() {
  return ::IsWindowEnabled(window_->hwnd());
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
  if (transparent()) {
    SetBounds(restore_bounds_, false);
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
  if (IsVisible()) {
    window_->Minimize();
  } else {
    const bool was_visible = IsVisible();
    window_->Show(ui::SHOW_STATE_MINIMIZED, gfx::Rect());
    if (!was_visible && is_modal() && NativeWindow::parent()) {
      parent()->IncrementChildModals();
    }
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

  // TODO(Guo Xi): Here is the menubar-related code, with reference to Electron.
}

bool NativeWindowWin::IsFullscreen() const {
  return window_->IsFullscreen();
}

void NativeWindowWin::SetBounds(const gfx::Rect& bounds, bool animate) {
  if (!CanResize()) {
    SetMaximumSize(bounds.size());
    SetMinimumSize(bounds.size());
  }

  window_->SetBounds(bounds, true);
}

gfx::Rect NativeWindowWin::GetBounds() const {
  if (IsMinimized()) {
    return window_->GetRestoredBounds();
  }

  gfx::Rect bounds = window_->GetWindowBoundsInScreen();
  return bounds;
}

float NativeWindowWin::GetDevicePixelRatio() const {
  return GetDPIForHWND(GetNativeWindowHandle()) / kDefaultDPI;
}

gfx::Rect NativeWindowWin::GetNormalBounds() const {
  if (IsMaximized() && transparent()) {
    return restore_bounds_;
  }
  return window_->GetRestoredBounds();
}

void NativeWindowWin::SetResizable(bool resizable) {
  if (resizable != this->resizable()) {
    set_resizable(resizable);
    if (resizable) {
      SetContentSizeConstraints(old_size_constraints_);
    } else {
      old_size_constraints_ = GetContentSizeConstraints();
      gfx::Size content_size = GetContentSize();
      SetContentSizeConstraints(SizeConstraints(content_size, content_size));
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
  gfx::Size screen_size = DIPToScreenSize(GetSize());
  window_->CenterWindow(screen_size);
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

// TODO(Guo Xi): continue reviewing the code from here.
// SkColor NativeWindowWin::GetBackgroundColor() {
//  auto* background = root_view_->background();
//  if (!background)
//    return SK_ColorTRANSPARENT;
//  return background->get_color();
//}
//
// void NativeWindowWin::SetBackgroundColor(SkColor background_color) {
//  // web views' background color.
//  root_view_->SetBackground(views::CreateSolidBackground(background_color));
//
//  // Set the background color of native window.
//  HBRUSH brush =
//  CreateSolidBrush(skia::SkColorToCOLORREF(background_color)); ULONG_PTR
//  previous_brush =
//      SetClassLongPtr(GetAcceleratedWidget(), GCLP_HBRBACKGROUND,
//                      reinterpret_cast<LONG_PTR>(brush));
//  if (previous_brush)
//    DeleteObject((HBRUSH)previous_brush);
//  InvalidateRect(GetAcceleratedWidget(), NULL, 1);
//}

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
  can_activate_ = focusable;
  LONG ex_style = ::GetWindowLong(GetNativeWindowHandle(), GWL_EXSTYLE);
  if (focusable) {
    ex_style &= ~WS_EX_NOACTIVATE;
  } else {
    ex_style |= WS_EX_NOACTIVATE;
  }
  ::SetWindowLong(GetNativeWindowHandle(), GWL_EXSTYLE, ex_style);
  //  SetSkipTaskbar(!focusable);
  Focus(false);
}

bool NativeWindowWin::IsFocusable() const {
  LONG ex_style = ::GetWindowLong(GetNativeWindowHandle(), GWL_EXSTYLE);
  bool no_activate = ex_style & WS_EX_NOACTIVATE;
  return !no_activate && can_activate_;
}

void NativeWindowWin::SetParentWindow(NativeWindow* parent) {
  NativeWindow* const old_parent = NativeWindow::parent();
  NativeWindow::SetParentWindow(parent);
  if (is_modal() && IsVisible() && old_parent != parent) {
    if (old_parent) {
      old_parent->DecrementChildModals();
    }
    if (parent) {
      parent->IncrementChildModals();
    }
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

  gfx::Rect window_bounds(bounds);
  HWND hwnd = GetNativeWindowHandle();
  gfx::Rect dpi_bounds = DIPToScreenRect(hwnd, bounds);
  window_bounds =
      ScreenToDIPRect(hwnd, GetWindowBoundsForClientBounds(hwnd, dpi_bounds));
  return window_bounds;
}

gfx::Rect NativeWindowWin::WindowBoundsToContentBounds(
    const gfx::Rect& bounds) const {
  if (!frame()) {
    return bounds;
  }

  gfx::Rect content_bounds(bounds);
  HWND hwnd = GetNativeWindowHandle();
  content_bounds.set_size(DIPToScreenRect(hwnd, content_bounds).size());
  RECT rect;
  SetRectEmpty(&rect);
  DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);
  DWORD ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
  AdjustWindowRectEx(&rect, style, FALSE, ex_style);
  content_bounds.set_width(content_bounds.width() - (rect.right - rect.left));
  content_bounds.set_height(content_bounds.height() - (rect.bottom - rect.top));
  content_bounds.set_size(ScreenToDIPRect(hwnd, content_bounds).size());
  return content_bounds;
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
  return can_activate_;
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
  auto constrains = GetSizeConstraints();
  *min_size = constrains.GetMinimumSize();
  *max_size = constrains.GetMaximumSize();
  if (max_size->IsEmpty()) {
    *max_size = gfx::Size(INT_MAX, INT_MAX);
  }
}

gfx::Size NativeWindowWin::GetRootViewSize() const {
  return gfx::Size();
}

gfx::Size NativeWindowWin::DIPToScreenSize(const gfx::Size& dip_size) const {
  return lynxtron::DIPToScreenSize(window_->hwnd(), dip_size);
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
  return false;
}

void NativeWindowWin::HandleCreate() {}

void NativeWindowWin::HandleDestroying() {}

void NativeWindowWin::HandleDisplayChange() {}

void NativeWindowWin::HandleBeginWMSizeMove() {}

void NativeWindowWin::HandleEndWMSizeMove() {}

void NativeWindowWin::HandleWorkAreaChanged() {}

void NativeWindowWin::HandleVisibilityChanged(bool visible) {}

void NativeWindowWin::HandleWindowMinimizedOrRestored(bool restored) {
  if (restored) {
    if (last_window_state_ != ui::SHOW_STATE_MINIMIZED) {
      return;
    }
    if (IsFullscreen()) {
      last_window_state_ = ui::SHOW_STATE_FULLSCREEN;
      NotifyWindowEnterFullScreen();
      return;
    }
    if (IsMaximized()) {
      return;
    }
    last_window_state_ = ui::SHOW_STATE_NORMAL;
    NotifyWindowRestore();
    return;
  }

  if (last_window_state_ == ui::SHOW_STATE_MINIMIZED) {
    return;
  }

  WINDOWPLACEMENT wp;
  wp.length = sizeof(WINDOWPLACEMENT);
  if (GetWindowPlacement(GetNativeWindowHandle(), &wp)) {
    last_normal_placement_bounds_ = gfx::Rect(wp.rcNormalPosition);
  }

  last_window_state_ = ui::SHOW_STATE_MINIMIZED;
  NotifyWindowMinimize();
}

void NativeWindowWin::HandleWindowMaximized(bool maximized) {
  if (maximized) {
    if (last_window_state_ == ui::SHOW_STATE_MAXIMIZED) {
      return;
    }

    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(GetNativeWindowHandle(), &wp)) {
      last_normal_placement_bounds_ = gfx::Rect(wp.rcNormalPosition);
    }

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
  NotifyWindowUnmaximize();
}

void NativeWindowWin::HandleFrameChanged() {}

void NativeWindowWin::HandleNativeFocus(HWND last_focused_window) {}

void NativeWindowWin::HandleNativeBlur(HWND focused_window) {}

void NativeWindowWin::HandleMenuLoop(bool in_menu_loop) {}

void NativeWindowWin::HandleWindowSizeChanging() {}

void NativeWindowWin::HandleWindowSizeUnchanged() {}

void NativeWindowWin::HandleDestroyed() {
  if (is_modal() && NativeWindow::parent()) {
    static_cast<NativeWindowWin*>(parent())->DecrementChildModals();
    parent()->Focus(true);
  }
  NotifyWindowClosed();
}

bool NativeWindowWin::HandleInitialFocus(ui::WindowShowState show_state) {
  // Need to focus on LynxView window handle, so LynxView can get keyboard
  // message
  return can_activate_;
}

void NativeWindowWin::HandleMove() {
  NotifyWindowMove();
}

bool NativeWindowWin::HandleMoving(RECT* rect) {
  is_moving_ = true;
  bool prevent_default = false;
  gfx::Rect bounds = gfx::Rect(*rect);
  HWND hwnd = GetNativeWindowHandle();
  gfx::Rect dpi_bounds = ScreenToDIPRect(hwnd, bounds);
  NotifyWindowWillMove(dpi_bounds, prevent_default);
  if (!movable_ || prevent_default) {
    ::GetWindowRect(hwnd, rect);
    return true;  // Tells Windows that the Move is handled. If not
    // true,
    // frameless windows can be moved using
    // -webkit-app-region: drag elements.
  }
  return false;
  // NotifyWindowMove();
  // return true;
}

void NativeWindowWin::HandleClientSizeChanged(const gfx::Size& new_size) {
  NotifyWindowResize();
}

void NativeWindowWin::Maximize() {
  if (!transparent()) {
    if (IsVisible()) {
      window_->Maximize();
    } else {
      const bool was_visible = IsVisible();
      window_->Show(ui::SHOW_STATE_MAXIMIZED, gfx::Rect());
      if (!was_visible && is_modal() && NativeWindow::parent()) {
        parent()->IncrementChildModals();
      }
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
