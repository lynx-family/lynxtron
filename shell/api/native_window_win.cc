// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/api/native_window_win.h"

#include <base/logging.h>
#include <wrl/client.h>

#include <memory>
#include <utility>
#include <vector>

#include "Shobjidl.h"
#include "base/containers/contains.h"
#include "base/i18n/rtl.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "shell/api/dpi_win.h"
#include "shell/app/javascript_environment.h"
#include "shell/app/window_list.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/options_switches.h"
#include "shell/ui/gfx/geometry/resize_utils.h"
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

// Chromium uses a buggy implementation that converts content rect to window
// rect when calculating min/max size, we should use the same implementation
// when passing min/max size so we can get correct results.
gfx::Size WindowSizeToContentSizeBuggy(HWND hwnd, const gfx::Size& size) {
  // Calculate the size of window frame, using same code with the
  // HWNDMessageHandler::OnGetMinMaxInfo method.
  // The pitfall is, when window is minimized the calculated window frame size
  // will be different from other states.
  RECT client_rect, rect;
  GetClientRect(hwnd, &client_rect);
  GetWindowRect(hwnd, &rect);
  CR_DEFLATE_RECT(&rect, &client_rect);
  // Convert DIP size to pixel size, do calculation and then return DIP size.
  gfx::Rect screen_rect = DIPToScreenRect(hwnd, gfx::Rect(size));
  gfx::Size screen_client_size(screen_rect.width() - (rect.right - rect.left),
                               screen_rect.height() - (rect.bottom - rect.top));
  return ScreenToDIPRect(hwnd, gfx::Rect(screen_client_size)).size();
}

}  // namespace

NativeWindowWin::NativeWindowWin(const gin_helper::Dictionary& options,
                                 NativeWindow* parent)
    : NativeWindow(options, parent),
      window_(std::make_unique<HWNDMessageHandler>(this)) {
  options.Get(options::kTitle, &title_);

  // On Windows we rely on the CanResize() to indicate whether window can be
  // resized, and it should be set before window is created.
  options.Get(options::kResizable, &resizable_);
  options.Get(options::kMinimizable, &minimizable_);
  options.Get(options::kMaximizable, &maximizable_);
  options.Get(options::kFocusable, &can_activate_);

  // Transparent window must not have thick frame.
  options.Get("thickFrame", &thick_frame_);
  if (transparent()) {
    thick_frame_ = false;
    window_->set_is_translucent(true);
  }

  if (title_bar_style_ != TitleBarStyle::kNormal) {
    set_has_frame(false);
  }

  bool fullscreen = false;
  options.Get(options::kFullscreen, &fullscreen);

  std::string window_type;
  options.Get(options::kType, &window_type);

  int x = -1, y = -1;
  options.Get(options::kX, &x);
  options.Get(options::kY, &y);

  int width = 800, height = 600;
  options.Get(options::kWidth, &width);
  options.Get(options::kHeight, &height);
  gfx::Rect bounds(x, y, width, height);
  bounds = DIPToScreenRect(GetNativeWindowHandle(), bounds);

  DWORD frame_style =
      WS_CAPTION | WS_OVERLAPPED | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
  if (resizable_) {
    frame_style |= WS_THICKFRAME;
  }
  window_->set_window_style(frame_style);

  // Save initial window state.
  if (fullscreen) {
    last_window_state_ = ui::SHOW_STATE_FULLSCREEN;
  } else {
    last_window_state_ = ui::SHOW_STATE_NORMAL;
  }

  window_->Init(HWND_DESKTOP, bounds);

  // window_->SizeConstraintsChanged();

  //   if (has_frame()) {
  // #if defined(OS_WIN)
  //     // thickFrame also works for normal window.
  //     if (!thick_frame_)
  //       FlipWindowStyle(GetNativeWindowHandle(), false, WS_THICKFRAME);
  // #endif
  //   }
}

NativeWindowWin::~NativeWindowWin() {
  if (is_modal() && NativeWindow::parent()) {
    static_cast<NativeWindowWin*>(parent())->DecrementChildModals();
    parent()->Focus(true);
  }
}

void NativeWindowWin::Close() {
  if (!IsClosable()) {
    WindowList::WindowCloseCancelled(this);
    return;
  }

  // TODO(Guo Xi): review related logic
  window_->Close();
  NotifyWindowCloseButtonClicked();
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
  if (is_modal() && NativeWindow::parent() && !IsVisible()) {
    parent()->IncrementChildModals();
  }

  window_->Show(ui::WindowShowState::SHOW_STATE_NORMAL, gfx::Rect());
  //// explicitly focus the window
  window_->Activate();

  NotifyWindowShow();
}

void NativeWindowWin::ShowInactive() {
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
#if defined(OS_WIN)
  // When the window is removed from the taskbar via win.hide(),
  // the thumbnail buttons need to be set up again.
  // Ensure that when the window is hidden,
  // the taskbar host is notified that it should re-add them.
  /// taskbar_host_.SetThumbarButtonsAdded(false);
#endif
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

#if defined(OS_LINUX)
void NativeWindowWin::Maximize() {
  if (IsVisible()) {
    widget()->Maximize();
  } else {
    widget()->native_widget_private()->Show(ui::SHOW_STATE_MAXIMIZED,
                                            gfx::Rect());
  }
}
#endif

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

  // TODO(Guo Xi): Add description here
  //   #if BUILDFLAG(IS_WIN)
  //   if (transparent() && !IsMinimized()) {
  //     // If the window is the same dimensions and placement as the
  //     // display, we consider it maximized.
  //     auto display =
  //         display::Screen::Get()->GetDisplayNearestWindow(GetNativeWindow());
  //     return GetBounds() == display.work_area();
  //   }
  // #endif
}

void NativeWindowWin::Minimize() {
  if (IsVisible()) {
    window_->Minimize();
  } else {
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

  // TODO(Guo Xi): Here is the menubar-related code, with reference to Electron.
}

bool NativeWindowWin::IsFullscreen() const {
  return window_->IsFullscreen();
}

void NativeWindowWin::SetBounds(const gfx::Rect& bounds, bool animate) {
  if (is_moving_ || is_resizing_) {
    pending_bounds_change_ = bounds;
  }

  if (!CanResize()) {
    SetMaximumSize(bounds.size());
    SetMinimumSize(bounds.size());
  }

  // auto new_bounds = DIPToScreenRect(GetNativeWindowHandle(), bounds);
  window_->SetBounds(bounds, true);
}

gfx::Rect NativeWindowWin::GetBounds() const {
  if (IsMinimized()) {
    return window_->GetRestoredBounds();
  }

  gfx::Rect bounds = window_->GetWindowBoundsInScreen();
  // auto new_bounds = ScreenToDIPRect(GetNativeWindowHandle(), bounds);
  return bounds;
}

float NativeWindowWin::GetDevicePixelRatio() const {
  return GetDPIForHWND(GetNativeWindowHandle()) / kDefaultDPI;
}

// gfx::Size NativeWindowWin::GetSize() const {
//   return DIPToScreenSize(GetSize());
// }

// TODO(Guo Xi): support content bounds/content size
// gfx::Rect NativeWindowViews::GetContentBounds() const {
//   return content_view() ? content_view()->GetBoundsInScreen() : gfx::Rect();
// }

// gfx::Size NativeWindowViews::GetContentSize() const {
// #if BUILDFLAG(IS_WIN)
//   if (IsMinimized())
//     return NativeWindow::GetContentSize();
// #endif

//   return content_view() ? content_view()->size() : gfx::Size();
// }

gfx::Rect NativeWindowWin::GetNormalBounds() const {
  if (IsMaximized() && transparent()) {
    return restore_bounds_;
  }
  return window_->GetRestoredBounds();
}

void NativeWindowWin::SetResizable(bool resizable) {
  if (resizable != resizable_) {
    resizable_ = resizable;
    if (resizable) {
      // SetContentSizeConstraints(old_size_constraints_);
    } else {
      // old_size_constraints_ = GetContentSizeConstraints();
      // gfx::Size content_size = GetContentSize();
      // SetContentSizeConstraints(SizeConstraints(content_size, content_size));
    }
  }
}

void NativeWindowWin::MoveTop() {
  window_->StackAtTop();
}

bool NativeWindowWin::IsResizable() const {
  if (has_frame()) {
    return ::GetWindowLong(GetNativeWindowHandle(), GWL_STYLE) & WS_THICKFRAME;
  }

  return resizable_;
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
  minimizable_ = minimizable;
}

bool NativeWindowWin::IsMinimizable() const {
  return ::GetWindowLong(GetNativeWindowHandle(), GWL_STYLE) & WS_MINIMIZEBOX;
}

void NativeWindowWin::SetMaximizable(bool maximizable) {
  FlipWindowStyle(GetNativeWindowHandle(), maximizable, WS_MAXIMIZEBOX);
  maximizable_ = maximizable;
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

void NativeWindowWin::SetKiosk(bool kiosk) {
  SetFullScreen(kiosk);
}

bool NativeWindowWin::IsKiosk() const {
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
// #if defined(OS_WIN)
//  // Set the background color of native window.
//  HBRUSH brush =
//  CreateSolidBrush(skia::SkColorToCOLORREF(background_color)); ULONG_PTR
//  previous_brush =
//      SetClassLongPtr(GetAcceleratedWidget(), GCLP_HBRBACKGROUND,
//                      reinterpret_cast<LONG_PTR>(brush));
//  if (previous_brush)
//    DeleteObject((HBRUSH)previous_brush);
//  InvalidateRect(GetAcceleratedWidget(), NULL, 1);
// #endif
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
#if defined(OS_WIN)
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
#else
  opacity_ = 1.0;  // setOpacity unsupported on Linux
#endif
}

double NativeWindowWin::GetOpacity() {
  return opacity_;
}

// void NativeWindowWin::SetIgnoreMouseEvents(bool ignore, bool forward) {
//   LONG ex_style = ::GetWindowLong(GetNativeWindowHandle(), GWL_EXSTYLE);
//   if (ignore) {
//     ex_style |= (WS_EX_TRANSPARENT | WS_EX_LAYERED);
//   } else {
//     ex_style &= ~(WS_EX_TRANSPARENT | WS_EX_LAYERED);
//   }
//   if (layered_) {
//     ex_style |= WS_EX_LAYERED;
//   }
//   ::SetWindowLong(GetNativeWindowHandle(), GWL_EXSTYLE, ex_style);

//   // Forwarding is always disabled when not ignoring mouse messages.
//   // if (!ignore) {
//   //   SetForwardMouseMessages(false);
//   // } else {
//   //   SetForwardMouseMessages(forward);
//   // }
// }

void NativeWindowWin::SetContentProtection(bool enable) {
  // #if defined(OS_WIN)
  //   HWND hwnd = GetAcceleratedWidget();
  //   if (!layered_) {
  //     // Workaround to prevent black window on screen capture after hiding
  //     and
  //     // showing the BrowserWindow.
  //     LONG ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
  //     ex_style |= WS_EX_LAYERED;
  //     ::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);
  //     layered_ = true;
  //   }
  //   DWORD affinity = enable ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
  //   ::SetWindowDisplayAffinity(hwnd, affinity);
  // #endif
}

void NativeWindowWin::SetFocusable(bool focusable) {
  can_activate_ = focusable;
#if defined(OS_WIN)
  LONG ex_style = window_->window_ex_style();
  if (focusable) {
    ex_style &= ~WS_EX_NOACTIVATE;
  } else {
    ex_style |= WS_EX_NOACTIVATE;
  }
  ::SetWindowLong(GetNativeWindowHandle(), GWL_EXSTYLE, ex_style);
  //  SetSkipTaskbar(!focusable);
  Focus(false);
#endif
}

bool NativeWindowWin::IsFocusable() const {
#if defined(OS_WIN)
  LONG ex_style = window_->window_ex_style();
  bool no_activate = ex_style & WS_EX_NOACTIVATE;
  return !no_activate && can_activate_;
#else
  return can_activate;
#endif
}

// void NativeWindowWin::SetMenu(LynxtronMenuModel* menu_model) {
// #if defined(USE_X11)
//  // if (!features::IsUsingOzonePlatform()) {
//  //   // Remove global menu bar.
//  //   if (global_menu_bar_ && menu_model == nullptr) {
//  //     global_menu_bar_.reset();
//  //     root_view_->UnregisterAcceleratorsWithFocusManager();
//  //     return;
//  //   }
//
//  //   // Use global application menu bar when possible.
//  //   if (ShouldUseGlobalMenuBar()) {
//  //     if (!global_menu_bar_)
//  //       global_menu_bar_ = std::make_unique<GlobalMenuBarX11>(this);
//  //     if (global_menu_bar_->IsServerStarted()) {
//  //       root_view_->RegisterAcceleratorsWithFocusManager(menu_model);
//  //       global_menu_bar_->SetMenu(menu_model);
//  //       return;
//  //     }
//  //   }
//  // }
// #endif
//
//  // Should reset content size when setting menu.
//  //gfx::Size content_size = GetContentSize();
//  //bool should_reset_size = use_content_size_ && has_frame() &&
//  //                         !IsMenuBarAutoHide() &&
//  //                         ((!!menu_model) != root_view_->HasMenu());
//
//  //root_view_->SetMenu(menu_model);
//
//  //if (should_reset_size) {
//  //  // Enlarge the size constraints for the menu.
//  //  int menu_bar_height = root_view_->GetMenuBarHeight();
//  //  extensions::SizeConstraints constraints = GetContentSizeConstraints();
//  //  if (constraints.HasMinimumSize()) {
//  //    gfx::Size min_size = constraints.GetMinimumSize();
//  //    min_size.set_height(min_size.height() + menu_bar_height);
//  //    constraints.set_minimum_size(min_size);
//  //  }
//  //  if (constraints.HasMaximumSize()) {
//  //    gfx::Size max_size = constraints.GetMaximumSize();
//  //    max_size.set_height(max_size.height() + menu_bar_height);
//  //    constraints.set_maximum_size(max_size);
//  //  }
//  //  SetContentSizeConstraints(constraints);
//
//  //  // Resize the window to make sure content size is not changed.
//  //  SetContentSize(content_size);
//  //}
//}
//

void NativeWindowWin::SetParentWindow(NativeWindow* parent) {}

void NativeWindowWin::SetVisibleOnAllWorkspaces(bool visible,
                                                bool visibleonfullscreen,
                                                bool skiptransformprocesstype) {
}

bool NativeWindowWin::IsVisibleOnAllWorkspaces() {
  return false;
}

NativeWindowHandle NativeWindowWin::GetNativeWindowHandle() const {
#if defined(OS_WIN)
  return window_->hwnd();
#else
  return nullptr;
#endif
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
#if defined(OS_WIN)
  if (behind_task_bar_) {
    const HWND task_bar_hwnd = ::FindWindow(kUniqueTaskBarClassName, nullptr);
    ::SetWindowPos(GetNativeWindowHandle(), task_bar_hwnd, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
#endif
  // TODO(julien.isorce): Implement X11 case.
}

// static
// NativeWindow* NativeWindowWin::Create(
//    const gin_helper::Dictionary& options,
//    NativeWindow* parent) {
//  return new NativeWindowWin(options, parent);
//}

ui::WindowShowState NativeWindowWin::GetRestoredState() {
  if (IsMaximized()) {
#if defined(OS_WIN)
    // Only restore Maximized state when window is NOT transparent style
    if (!transparent()) {
      return ui::SHOW_STATE_MAXIMIZED;
    }
#else
    return ui::SHOW_STATE_MAXIMIZED;
#endif
  }

  if (IsFullscreen()) {
    return ui::SHOW_STATE_FULLSCREEN;
  }

  return ui::SHOW_STATE_NORMAL;
}

gfx::Rect NativeWindowWin::ContentBoundsToWindowBounds(
    const gfx::Rect& bounds) const {
  if (!has_frame()) {
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
  if (!has_frame()) {
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

// bool NativeWindowWin::HasNonClientView() const {
//   return !thick_frame_;
// }

// TODO(Guo Xi): frame mode
ui::FrameMode NativeWindowWin::GetFrameMode() const {
  return ui::FrameMode::SYSTEM_DRAWN;
}

bool NativeWindowWin::HasFrame() const {
  return NativeWindow::has_frame();
}

bool NativeWindowWin::ShouldPaintAsActive() const {
  return false;
}

bool NativeWindowWin::CanResize() const {
  return resizable_ && thick_frame_;
}

bool NativeWindowWin::CanMaximize() const {
  return maximizable_;
}

bool NativeWindowWin::CanMinimize() const {
  return minimizable_;
}

bool NativeWindowWin::CanActivate() const {
  return true;
}

bool NativeWindowWin::WantsMouseEventsWhenInactive() const {
  return false;
}

bool NativeWindowWin::IsModal() const {
  return false;
}

int NativeWindowWin::GetInitialShowState() const {
  return ui::WindowShowState::SHOW_STATE_NORMAL;
}

// int NativeWindowWin::GetNonClientComponent(
//     const gfx::Point& point) const {
//   return HTNOWHERE;
// }

bool NativeWindowWin::GetClientAreaInsets(gfx::Insets* insets,
                                          HMONITOR monitor) const {
  LOG(INFO) << " GetClientAreaInsets";
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

void NativeWindowWin::HandleCaptureLost() {}

void NativeWindowWin::HandleClose() {
  NotifyWindowCloseButtonClicked();
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

void NativeWindowWin::HandleWindowMinimizedOrRestored(bool restored) {}

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

// void NativeWindowWin::HandleInputLanguageChange(DWORD character_set,
//                                                 HKL input_language_id) {}

// bool NativeWindowWin::HandleTooltipNotify(int w_param,
//                                           NMHDR* l_param,
//                                           LRESULT* l_result) {
//   return false;
// }

// Copied from ui/views/win/hwnd_message_handler.cc
gfx::ResizeEdge GetWindowResizeEdge(WPARAM param) {
  switch (param) {
    case WMSZ_BOTTOM:
      return gfx::ResizeEdge::kBottom;
    case WMSZ_TOP:
      return gfx::ResizeEdge::kTop;
    case WMSZ_LEFT:
      return gfx::ResizeEdge::kLeft;
    case WMSZ_RIGHT:
      return gfx::ResizeEdge::kRight;
    case WMSZ_TOPLEFT:
      return gfx::ResizeEdge::kTopLeft;
    case WMSZ_TOPRIGHT:
      return gfx::ResizeEdge::kTopRight;
    case WMSZ_BOTTOMLEFT:
      return gfx::ResizeEdge::kBottomLeft;
    case WMSZ_BOTTOMRIGHT:
      return gfx::ResizeEdge::kBottomRight;
    default:
      return gfx::ResizeEdge::kBottomRight;
  }
}

// std::set<NativeWindowWin*> NativeWindowWin::forwarding_windows_;
// TODO(Guo Xi): whether we still need mouse_hook_
HHOOK NativeWindowWin::mouse_hook_ = NULL;

void NativeWindowWin::Maximize() {
  if (!transparent()) {
    if (IsVisible()) {
      window_->Maximize();
    } else {
      window_->Show(ui::SHOW_STATE_MAXIMIZED, gfx::Rect());
      NotifyWindowShow();
    }
  } else {
    // TODO(Kang Hongjia): No plan for supporting transparent window.
    //  restore_bounds_ = GetBounds();
    //  auto display = display::Screen::GetScreen()->GetDisplayNearestWindow(
    //      GetNativeWindow());
    //  SetBounds(display.work_area(), false);
  }
}

// bool NativeWindowWin::ExecuteWindowsCommand(int command_id) {
//  std::string command = AppCommandToString(command_id);
//  NotifyWindowExecuteAppCommand(command);
//
//  return false;
//}

bool NativeWindowWin::PreHandleMSG(UINT message,
                                   WPARAM w_param,
                                   LPARAM l_param,
                                   LRESULT* result) {
  NotifyWindowMessage(message, w_param, l_param);

  // // Avoid side effects when calling SetWindowPlacement.
  // if (is_setting_window_placement_) {
  //   // Let Chromium handle the WM_NCCALCSIZE message otherwise the window
  //   size
  //   // would be wrong.
  //   // See https://github.com/electron/electron/issues/22393 for more.
  //   if (message == WM_NCCALCSIZE)
  //     return false;
  //   // Otherwise handle the message with default proc,
  //   *result = DefWindowProc(GetNativeWindowHandle(), message, w_param,
  //   l_param);
  //   // and tell Chromium to ignore this message.
  //   return true;
  // }

  switch (message) {
      //   // Screen readers send WM_GETOBJECT in order to get the
      //   accessibility
      //   // object, so take this opportunity to push Chromium into
      //   accessible
      //   // mode if it isn't already, always say we didn't handle the
      //   message
      //   // because we still want Chromium to handle returning the actual
      //   // accessibility object.
      //   case WM_GETOBJECT: {
      //     if (checked_for_a11y_support_)
      //       return false;

      //     const DWORD obj_id = static_cast<DWORD>(l_param);

      //     if (obj_id != static_cast<DWORD>(OBJID_CLIENT)) {
      //       return false;
      //     }

      //     if (!IsScreenReaderActive()) {
      //       return false;
      //     }

      //     checked_for_a11y_support_ = true;

      //     // auto* const axState =
      //     // content::BrowserAccessibilityState::GetInstance(); if (axState
      //     &&
      //     // !axState->IsAccessibleBrowser()) {
      //     //   axState->OnScreenReaderDetected();
      //     //   Browser::Get()->OnAccessibilitySupportChanged();
      //     // }

      //     return false;
      //   }
      //   case WM_GETMINMAXINFO: {
      //     WINDOWPLACEMENT wp;
      //     wp.length = sizeof(WINDOWPLACEMENT);

      //     // We do this to work around a Windows bug, where the minimized
      //     Window
      //     // would report that the closest display to it is not the one
      //     that it was
      //     // previously on (but the leftmost one instead). We restore the
      //     position
      //     // of the window during the restore operation, this way chromium
      //     can
      //     // use the proper display to calculate the scale factor to use.
      //     if (!last_normal_placement_bounds_.IsEmpty() &&
      //         (IsVisible() || IsMinimized()) &&
      //         GetWindowPlacement(GetNativeWindowHandle(), &wp)) {
      //       wp.rcNormalPosition = last_normal_placement_bounds_.ToRECT();

      //       // When calling SetWindowPlacement, Chromium would do window
      //       messages
      //       // handling. But since we are already in PreHandleMSG this
      //       would cause
      //       // crash in Chromium under some cases.
      //       //
      //       // We work around the crash by prevent Chromium from handling
      //       window
      //       // messages until the SetWindowPlacement call is done.
      //       //
      //       // See https://github.com/electron/electron/issues/21614 for
      //       more. is_setting_window_placement_ = true;
      //       SetWindowPlacement(GetNativeWindowHandle(), &wp);
      //       is_setting_window_placement_ = false;

      //       last_normal_placement_bounds_ = gfx::Rect();
      //     }

      //     return false;
      //   }
      //     // case WM_COMMAND:
      //     //  // Handle thumbar button click message.
      //     //  if (HIWORD(w_param) == THBN_CLICKED)
      //     //    return
      //     taskbar_host_.HandleThumbarButtonEvent(LOWORD(w_param));
      //     //  return false;

    // case WM_SIZING: {
    //   is_resizing_ = true;
    //   bool prevent_default = false;
    //   gfx::Rect bounds = gfx::Rect(*reinterpret_cast<RECT*>(l_param));
    //   HWND hwnd = GetNativeWindowHandle();
    //   gfx::Rect dpi_bounds = ScreenToDIPRect(hwnd, bounds);
    //   NotifyWindowWillResize(dpi_bounds, GetWindowResizeEdge(w_param),
    //                          prevent_default);
    //   if (prevent_default) {
    //     ::GetWindowRect(hwnd, reinterpret_cast<RECT*>(l_param));
    //     return true;  // Tells Windows that the Sizing is handled.
    //   }
    //   return false;
    // }
    case WM_SIZE: {
      // Handle window state change.
      HandleSizeEvent(w_param, l_param);
      return false;
    }
    case WM_EXITSIZEMOVE: {
      if (is_resizing_) {
        NotifyWindowResized();
        is_resizing_ = false;
      }
      if (is_moving_) {
        NotifyWindowMoved();
        is_moving_ = false;
      }
      return false;
    }
    // case WM_MOVING: {
    //   is_moving_ = true;
    //   bool prevent_default = false;
    //   gfx::Rect bounds = gfx::Rect(*reinterpret_cast<RECT*>(l_param));
    //   HWND hwnd = GetNativeWindowHandle();
    //   gfx::Rect dpi_bounds = ScreenToDIPRect(hwnd, bounds);
    //   NotifyWindowWillMove(dpi_bounds, prevent_default);
    //   if (!movable_ || prevent_default) {
    //     ::GetWindowRect(hwnd, reinterpret_cast<RECT*>(l_param));
    //     return true;  // Tells Windows that the Move is handled. If not
    //     // true,
    //     // frameless windows can be moved using
    //     // -webkit-app-region: drag elements.
    //   }
    //   return false;
    // }
    //   case WM_ENDSESSION: {
    //     if (w_param) {
    //       NotifyWindowEndSession();
    //     }
    //     return false;
    //   }
    //   case WM_PARENTNOTIFY: {
    //     if (LOWORD(w_param) == WM_CREATE) {
    //       // Because of reasons regarding legacy drivers and stuff, a
    //       window that
    //       // matches the client area is created and used internally by
    //       Chromium.
    //       // This is used when forwarding mouse messages. We only cache
    //       the first
    //       // occurrence (the webview window) because dev tools also
    //       cause this
    //       // message to be sent.
    //       if (!legacy_window_) {
    //         legacy_window_ = reinterpret_cast<HWND>(l_param);
    //       }
    //     }
    //     return false;
    //   }
    //   case WM_CONTEXTMENU: {
    //     bool prevent_default = false;
    //     NotifyWindowSystemContextMenu(GET_X_LPARAM(l_param),
    //                                   GET_Y_LPARAM(l_param),
    //                                   &prevent_default);
    //     return prevent_default;
    //   }
    //   case WM_SYSCOMMAND: {
    //     // Mask is needed to account for double clicking title bar to
    //     maximize WPARAM max_mask = 0xFFF0; if (transparent() &&
    //     ((w_param & max_mask) == SC_MAXIMIZE)) {
    //       return true;
    //     }
    //     return false;
    //   }
    //   case WM_INITMENU: {
    //     // This is handling the scenario where the menu might get
    //     triggered by the
    //     // user doing "alt + space" resulting in system maximization
    //     and restore
    //     // being used on transparent windows when that does not work.
    //     if (transparent()) {
    //       HMENU menu = GetSystemMenu(GetNativeWindowHandle(), false);
    //       EnableMenuItem(menu, SC_MAXIMIZE,
    //                      MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
    //       EnableMenuItem(menu, SC_RESTORE,
    //                      MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
    //       return true;
    //     }
    //     return false;
    //   }
    // case WM_ACTIVATE: {
    //   if (!!w_param) {
    //     NotifyWindowFocus();
    //     return true;
    //   } else {
    //     NotifyWindowBlur();
    //   }
    //   return false;
    // }
    //   // case WM_NCCALCSIZE: {
    //   //   *result =
    //   //       DefWindowProc(GetNativeWindowHandle(), message, w_param,
    //   l_param);
    //   //   return true;
    //   // }
    default: {
      return false;
    }
  }
}

void NativeWindowWin::PostHandleMSG(UINT message,
                                    WPARAM w_param,
                                    LPARAM l_param) {}

void NativeWindowWin::HandleWindowScaleFactorChanged(
    float window_scale_factor) {}

void NativeWindowWin::HandleSizeEvent(WPARAM w_param, LPARAM l_param) {
  // Here we handle the WM_SIZE event in order to figure out what is the
  // current window state and notify the user accordingly.
  switch (w_param) {
    case SIZE_MAXIMIZED:
    case SIZE_MINIMIZED: {
      WINDOWPLACEMENT wp;
      wp.length = sizeof(WINDOWPLACEMENT);

      if (GetWindowPlacement(GetNativeWindowHandle(), &wp)) {
        last_normal_placement_bounds_ = gfx::Rect(wp.rcNormalPosition);
      }

      // Note that SIZE_MAXIMIZED and SIZE_MINIMIZED might be emitted for
      // multiple times for one resize because of the SetWindowPlacement call.
      if (w_param == SIZE_MAXIMIZED &&
          last_window_state_ != ui::SHOW_STATE_MAXIMIZED) {
        last_window_state_ = ui::SHOW_STATE_MAXIMIZED;
        NotifyWindowMaximize();
      } else if (w_param == SIZE_MINIMIZED &&
                 last_window_state_ != ui::SHOW_STATE_MINIMIZED) {
        last_window_state_ = ui::SHOW_STATE_MINIMIZED;
        NotifyWindowMinimize();
      }
      break;
    }
    case SIZE_RESTORED:
      switch (last_window_state_) {
        case ui::SHOW_STATE_MAXIMIZED:
          last_window_state_ = ui::SHOW_STATE_NORMAL;
          NotifyWindowUnmaximize();
          break;
        case ui::SHOW_STATE_MINIMIZED:
          if (IsFullscreen()) {
            last_window_state_ = ui::SHOW_STATE_FULLSCREEN;
            NotifyWindowEnterFullScreen();
          } else {
            last_window_state_ = ui::SHOW_STATE_NORMAL;
            NotifyWindowRestore();
          }
          break;
        default:
          break;
      }
      break;
  }
}

// void NativeWindowWin::SetForwardMouseMessages(bool forward) {
//   if (forward && !forwarding_mouse_messages_) {
//     forwarding_mouse_messages_ = true;
//     forwarding_windows_.insert(this);

//     // Subclassing is used to fix some issues when forwarding mouse
//     messages;
//     // see comments in |SubclassProc|.
//     SetWindowSubclass(legacy_window_, SubclassProc, 1,
//                       reinterpret_cast<DWORD_PTR>(this));

//     if (!mouse_hook_) {
//       mouse_hook_ = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, NULL, 0);
//     }
//   } else if (!forward && forwarding_mouse_messages_) {
//     forwarding_mouse_messages_ = false;
//     forwarding_windows_.erase(this);

//     RemoveWindowSubclass(legacy_window_, SubclassProc, 1);

//     if (forwarding_windows_.empty()) {
//       UnhookWindowsHookEx(mouse_hook_);
//       mouse_hook_ = NULL;
//     }
//   }
// }

// LRESULT CALLBACK NativeWindowWin::MouseHookProc(int n_code,
//                                                 WPARAM w_param,
//                                                 LPARAM l_param) {
//   if (n_code < 0) {
//     return CallNextHookEx(NULL, n_code, w_param, l_param);
//   }

//   // Post a WM_MOUSEMOVE message for those windows whose client area
//   contains
//   // the cursor since they are in a state where they would otherwise ignore
//   all
//   // mouse input.
//   if (w_param == WM_MOUSEMOVE) {
//     for (auto* window : forwarding_windows_) {
//       // At first I considered enumerating windows to check whether the
//       cursor
//       // was directly above the window, but since nothing bad seems to
//       happen
//       // if we post the message even if some other window occludes it I
//       have
//       // just left it as is.
//       RECT client_rect;
//       GetClientRect(window->legacy_window_, &client_rect);
//       POINT p = reinterpret_cast<MSLLHOOKSTRUCT*>(l_param)->pt;
//       ScreenToClient(window->legacy_window_, &p);
//       if (PtInRect(&client_rect, p)) {
//         WPARAM w = 0;  // No virtual keys pressed for our purposes
//         LPARAM l = MAKELPARAM(p.x, p.y);
//         PostMessage(window->legacy_window_, WM_MOUSEMOVE, w, l);
//       }
//     }
//   }

//   return CallNextHookEx(NULL, n_code, w_param, l_param);
// }

// static
NativeWindow* NativeWindow::Create(const gin_helper::Dictionary& options,
                                   NativeWindow* parent) {
  return new NativeWindowWin(options, parent);
}

}  // namespace lynxtron
