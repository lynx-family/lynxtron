// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/ui/platform_window/win/hwnd_message_handler.h"

#include <dwmapi.h>
#include <shellapi.h>

#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/debug/gdi_debug_util_win.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util_win.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "base/win/windows_types.h"
#include "base/win/windows_version.h"
#include "shell/api/dpi_win.h"
#include "shell/common/win_util.h"
#include "shell/ui/base/win/shell.h"
#include "shell/ui/gfx/geometry/insets.h"
#include "shell/ui/gfx/geometry/resize_utils.h"
#include "shell/ui/gfx/win/hwnd_util.h"
#include "shell/ui/platform_window/win/fullscreen_handler.h"
#include "shell/ui/platform_window/win/hwnd_message_handler_delegate.h"
#include "shell/ui/platform_window/win/scoped_fullscreen_visibility.h"
#include "ui/display/win/screen_win.h"

namespace ui {

const wchar_t kWindowTranslucent[] = L"Chrome.WindowTranslucent";

namespace {

int GetFrameThickness(HMONITOR monitor) {
  // On Windows 10 the visible frame border is one pixel thick, but there is
  // some additional non-visible space: SM_CXSIZEFRAME (the resize handle)
  // and SM_CXPADDEDBORDER (additional border space that isn't part of the
  // resize handle).
  const int resize_frame_thickness =
      display::win::ScreenWin::GetSystemMetricsForMonitor(monitor,
                                                          SM_CXSIZEFRAME);
  const int padding_thickness =
      display::win::ScreenWin::GetSystemMetricsForMonitor(monitor,
                                                          SM_CXPADDEDBORDER);
  return resize_frame_thickness + padding_thickness;
}

bool GetMonitorAndRects(const RECT& rect,
                        HMONITOR* monitor,
                        gfx::Rect* monitor_rect,
                        gfx::Rect* work_area) {
  DCHECK(monitor);
  DCHECK(monitor_rect);
  DCHECK(work_area);
  *monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONULL);
  if (!*monitor) {
    return false;
  }
  MONITORINFO monitor_info = {0};
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(*monitor, &monitor_info);
  *monitor_rect = gfx::Rect(monitor_info.rcMonitor);
  *work_area = gfx::Rect(monitor_info.rcWork);
  return true;
}

// Enables or disables the menu item for the specified command and menu.
void EnableMenuItemByCommand(HMENU menu, UINT command, bool enabled) {
  UINT flags = MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED | MF_GRAYED);
  EnableMenuItem(menu, command, flags);
}

bool IsTopLevelWindow(HWND window) {
  LONG style = ::GetWindowLong(window, GWL_STYLE);
  if (!(style & WS_CHILD)) {
    return true;
  }
  HWND parent = ::GetParent(window);
  return !parent || (parent == ::GetDesktopWindow());
}

// Convert |param| to the gfx::ResizeEdge used in gfx::SizeRectToAspectRatio().
gfx::ResizeEdge GetWindowResizeEdge(UINT param) {
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
      // NOTREACHED();
      return gfx::ResizeEdge::kBottomRight;
  }
}

gfx::Size GetNonClientSizeInPixels(HWND hwnd) {
  RECT client_rect = {};
  RECT window_rect = {};
  if (!::GetClientRect(hwnd, &client_rect) ||
      !::GetWindowRect(hwnd, &window_rect)) {
    return gfx::Size();
  }

  const int client_width = client_rect.right - client_rect.left;
  const int client_height = client_rect.bottom - client_rect.top;
  const int window_width = window_rect.right - window_rect.left;
  const int window_height = window_rect.bottom - window_rect.top;

  return gfx::Size(std::max(0, window_width - client_width),
                   std::max(0, window_height - client_height));
}

constexpr int kAutoHideEdgeLeft = 1 << 0;
constexpr int kAutoHideEdgeTop = 1 << 1;
constexpr int kAutoHideEdgeRight = 1 << 2;
constexpr int kAutoHideEdgeBottom = 1 << 3;
constexpr int kAutoHideTaskbarThicknessPx = 2;

int AutoHideEdgeMaskFromAppbarEdge(UINT edge) {
  switch (edge) {
    case ABE_LEFT:
      return kAutoHideEdgeLeft;
    case ABE_TOP:
      return kAutoHideEdgeTop;
    case ABE_RIGHT:
      return kAutoHideEdgeRight;
    case ABE_BOTTOM:
      return kAutoHideEdgeBottom;
    default:
      return 0;
  }
}
}  // namespace

// static HWNDMessageHandler member initialization.
base::LazyInstance<HWNDMessageHandler::FullscreenWindowMonitorMap>::
    DestructorAtExit HWNDMessageHandler::fullscreen_monitor_map_ =
        LAZY_INSTANCE_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
// HWNDMessageHandler, public:

HWNDMessageHandler::HWNDMessageHandler(HWNDMessageHandlerDelegate* delegate)
    : delegate_(delegate),
      fullscreen_handler_(new FullscreenHandler),
      restored_enabled_(false),
      dpi_(0),
      called_enable_non_client_dpi_scaling_(false),
      ignore_window_pos_changes_(false),
      last_monitor_(nullptr),
      is_first_nccalc_(true),
      menu_depth_(0),
      dwm_composition_enabled_(ui::win::IsDwmCompositionEnabled()),
      sent_window_size_changing_(false),
      did_return_uia_object_(false),
      background_fullscreen_hack_(false) {}

HWNDMessageHandler::~HWNDMessageHandler() {
  // Prevent calls back into this class via WNDPROC now that we've been
  // destroyed.
  ClearUserData();
}

void HWNDMessageHandler::Init(HWND parent, const gfx::Rect& bounds) {
  GetMonitorAndRects(bounds.ToRECT(), &last_monitor_, &last_monitor_rect_,
                     &last_work_area_);

  // Create the window.
  WindowImpl::Init(parent, bounds);

  if (!called_enable_non_client_dpi_scaling_ && delegate_->HasFrame() &&
      lynxtron::IsProcessPerMonitorDpiAware()) {
    // Derived signature; not available in headers.
    // This call gets Windows to scale the non-client area when
    // WM_DPICHANGED is fired.
    using EnableChildWindowDpiMessagePtr = LRESULT(WINAPI*)(HWND, BOOL);
    static const auto enable_child_window_dpi_message_func =
        reinterpret_cast<EnableChildWindowDpiMessagePtr>(
            base::win::GetUser32FunctionPointer("EnableChildWindowDpiMessage"));
    if (enable_child_window_dpi_message_func) {
      enable_child_window_dpi_message_func(hwnd(), TRUE);
    }
  }
}

void HWNDMessageHandler::Close() {
  if (IsWindow(hwnd())) {
    Hide();
    CloseNow();
  }
}

void HWNDMessageHandler::CloseNow() {
  if (IsWindow(hwnd())) {
    DestroyWindow(hwnd());
  }
}

gfx::Rect HWNDMessageHandler::GetWindowBoundsInScreen() const {
  RECT r;
  GetWindowRect(hwnd(), &r);
  return gfx::Rect(r);
}

gfx::Rect HWNDMessageHandler::GetClientAreaBoundsInScreen() const {
  RECT r;
  GetClientRect(hwnd(), &r);
  POINT point = {r.left, r.top};
  ClientToScreen(hwnd(), &point);
  return gfx::Rect(point.x, point.y, r.right - r.left, r.bottom - r.top);
}

gfx::Rect HWNDMessageHandler::GetRestoredBounds() const {
  // If we're in fullscreen mode, we've changed the normal bounds to the monitor
  // rect, so return the saved bounds instead.
  if (IsFullscreen()) {
    return fullscreen_handler_->GetRestoreBounds();
  }

  gfx::Rect bounds;
  GetWindowPlacement(&bounds, nullptr);
  return bounds;
}

gfx::Rect HWNDMessageHandler::GetClientAreaBounds() const {
  if (IsMinimized()) {
    return gfx::Rect();
  }

  return GetWindowBoundsInScreen();
}

void HWNDMessageHandler::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  WINDOWPLACEMENT wp;
  wp.length = sizeof(wp);
  bool succeeded = !!::GetWindowPlacement(hwnd(), &wp);
  DCHECK(succeeded);

  if (bounds != nullptr) {
    if (wp.showCmd == SW_SHOWNORMAL) {
      // GetWindowPlacement can return misleading position if a normalized
      // window was resized using Aero Snap feature (see comment 9 in bug
      // 36421). As a workaround, using GetWindowRect for normalized windows.
      succeeded = GetWindowRect(hwnd(), &wp.rcNormalPosition) != 0;
      DCHECK(succeeded);

      *bounds = gfx::Rect(wp.rcNormalPosition);
    } else {
      MONITORINFO mi;
      mi.cbSize = sizeof(mi);
      succeeded =
          GetMonitorInfo(MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONEAREST),
                         &mi) != 0;
      DCHECK(succeeded);

      *bounds = gfx::Rect(wp.rcNormalPosition);
      // Convert normal position from workarea coordinates to screen
      // coordinates.
      bounds->Offset(mi.rcWork.left - mi.rcMonitor.left,
                     mi.rcWork.top - mi.rcMonitor.top);
    }
  }

  if (show_state) {
    if (wp.showCmd == SW_SHOWMAXIMIZED) {
      *show_state = ui::SHOW_STATE_MAXIMIZED;
    } else if (wp.showCmd == SW_SHOWMINIMIZED) {
      *show_state = ui::SHOW_STATE_MINIMIZED;
    } else {
      *show_state = ui::SHOW_STATE_NORMAL;
    }
  }
}

void HWNDMessageHandler::SetBounds(const gfx::Rect& bounds_in_pixels,
                                   bool force_size_changed) {
  background_fullscreen_hack_ = false;
  SetBoundsInternal(bounds_in_pixels, force_size_changed);
}

void HWNDMessageHandler::SetDwmFrameExtension(DwmFrameState state) {
  if (!delegate_->HasFrame() && ui::win::IsAeroGlassEnabled() &&
      !is_translucent_) {
    MARGINS m = {0, 0, 0, 0};
    if (state == DwmFrameState::kOn) {
      m = {0, 0, 1, 0};
    }
    DwmExtendFrameIntoClientArea(hwnd(), &m);
  }
}

void HWNDMessageHandler::SetSize(const gfx::Size& size) {
  SetWindowPos(hwnd(), nullptr, 0, 0, size.width(), size.height(),
               SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
}

void HWNDMessageHandler::CenterWindow(const gfx::Size& size) {
  HWND parent = GetParent(hwnd());
  if (!IsWindow(hwnd())) {
    parent = ::GetWindow(hwnd(), GW_OWNER);
  }
  gfx::CenterAndSizeWindow(parent, hwnd(), size);
}

void HWNDMessageHandler::StackAbove(HWND other_hwnd) {
  // Windows API allows to stack behind another windows only.
  DCHECK(other_hwnd);
  HWND next_window = GetNextWindow(other_hwnd, GW_HWNDPREV);
  SetWindowPos(hwnd(), next_window ? next_window : HWND_TOP, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
}

void HWNDMessageHandler::StackAtTop() {
  SetWindowPos(hwnd(), HWND_TOP, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
}

void HWNDMessageHandler::Show(ui::WindowShowState show_state,
                              const gfx::Rect& pixel_restore_bounds) {
  DWORD native_show_state;
  if (show_state == ui::SHOW_STATE_MAXIMIZED &&
      !pixel_restore_bounds.IsEmpty()) {
    WINDOWPLACEMENT placement = {0};
    placement.length = sizeof(WINDOWPLACEMENT);
    placement.showCmd = SW_SHOWMAXIMIZED;
    placement.rcNormalPosition = pixel_restore_bounds.ToRECT();
    SetWindowPlacement(hwnd(), &placement);
    native_show_state = SW_SHOWMAXIMIZED;
  } else {
    const bool is_maximized = IsMaximized();

    // Use SW_SHOW/SW_SHOWNA instead of SW_SHOWNORMAL/SW_SHOWNOACTIVATE so that
    // the window is not restored to its original position if it is maximized.
    // This could be used unconditionally for ui::SHOW_STATE_INACTIVE, but
    // cross-platform behavior when showing a minimized window is inconsistent,
    // some platforms restore the position, some do not. See crbug.com/1296710
    switch (show_state) {
      case ui::SHOW_STATE_INACTIVE:
        native_show_state = is_maximized ? SW_SHOWNA : SW_SHOWNOACTIVATE;
        break;
      case ui::SHOW_STATE_MAXIMIZED:
        native_show_state = SW_SHOWMAXIMIZED;
        break;
      case ui::SHOW_STATE_MINIMIZED:
        native_show_state = SW_SHOWMINIMIZED;
        break;
      case ui::SHOW_STATE_NORMAL:
        if ((GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_TRANSPARENT) ||
            (GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_NOACTIVATE)) {
          native_show_state = is_maximized ? SW_SHOWNA : SW_SHOWNOACTIVATE;
        } else {
          native_show_state = is_maximized ? SW_SHOW : SW_SHOWNORMAL;
        }
        break;
      case ui::SHOW_STATE_FULLSCREEN:
        native_show_state = SW_SHOWNORMAL;
        SetFullscreen(true);
        break;
      default:
        native_show_state = delegate_->GetInitialShowState();
        break;
    }

    ShowWindow(hwnd(), native_show_state);
    // When launched from certain programs like bash and Windows Live
    // Messenger, show_state is set to SW_HIDE, so we need to correct that
    // condition. We don't just change show_state to SW_SHOWNORMAL because
    // MSDN says we must always first call ShowWindow with the specified
    // value from STARTUPINFO, otherwise all future ShowWindow calls will be
    // ignored (!!#@@#!). Instead, we call ShowWindow again in this case.
    if (native_show_state == SW_HIDE) {
      native_show_state = SW_SHOWNORMAL;
      ShowWindow(hwnd(), native_show_state);
    }
  }

  // We need to explicitly activate the window if we've been shown with a state
  // that should activate, because if we're opened from a desktop shortcut while
  // an existing window is already running it doesn't seem to be enough to use
  // one of these flags to activate the window.
  if (native_show_state == SW_SHOWNORMAL ||
      native_show_state == SW_SHOWMAXIMIZED) {
    Activate();
  }

  if (!delegate_->HandleInitialFocus(show_state)) {
    SetInitialFocus();
  }
}

void HWNDMessageHandler::Hide() {
  if (IsWindow(hwnd())) {
    SetWindowPos(hwnd(), nullptr, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
                     SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
  }
}

void HWNDMessageHandler::Maximize() {
  ExecuteSystemMenuCommand(SC_MAXIMIZE);
}

void HWNDMessageHandler::Minimize() {
  ExecuteSystemMenuCommand(SC_MINIMIZE);
  delegate_->HandleNativeBlur(nullptr);
}

void HWNDMessageHandler::Restore() {
  ExecuteSystemMenuCommand(SC_RESTORE);
}

void HWNDMessageHandler::Activate() {
  if (IsMinimized()) {
    base::AutoReset<bool> restoring_activate(&notify_restore_on_activate_,
                                             true);
    ::ShowWindow(hwnd(), SW_RESTORE);
  }

  ::SetWindowPos(hwnd(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
  SetForegroundWindow(hwnd());
}

void HWNDMessageHandler::Deactivate() {
  HWND next_hwnd = ::GetNextWindow(hwnd(), GW_HWNDNEXT);
  while (next_hwnd) {
    if (::IsWindowVisible(next_hwnd)) {
      ::SetForegroundWindow(next_hwnd);
      return;
    }
    next_hwnd = ::GetNextWindow(next_hwnd, GW_HWNDNEXT);
  }
}

void HWNDMessageHandler::SetAlwaysOnTop(bool on_top) {
  ::SetWindowPos(hwnd(), on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

bool HWNDMessageHandler::IsVisible() const {
  return !!::IsWindowVisible(hwnd());
}

bool HWNDMessageHandler::IsActive() const {
  return GetActiveWindow() == hwnd();
}

bool HWNDMessageHandler::IsMinimized() const {
  return !!::IsIconic(hwnd());
}

bool HWNDMessageHandler::IsMaximized() const {
  return !!::IsZoomed(hwnd()) && !IsFullscreen();
}

bool HWNDMessageHandler::IsFullscreen() const {
  return fullscreen_handler_->fullscreen();
}

bool HWNDMessageHandler::IsAlwaysOnTop() const {
  return (GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

void HWNDMessageHandler::SendFrameChanged() {
  SetWindowPos(hwnd(), nullptr, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE |
                   SWP_NOOWNERZORDER | SWP_NOREPOSITION | SWP_NOSENDCHANGING |
                   SWP_NOSIZE | SWP_NOZORDER);
}

void HWNDMessageHandler::FlashFrame(bool flash) {
  FLASHWINFO fwi;
  fwi.cbSize = sizeof(fwi);
  fwi.hwnd = hwnd();
  if (flash) {
    fwi.dwFlags = FLASHW_ALL;
    fwi.uCount = 4;
    fwi.dwTimeout = 0;
  } else {
    fwi.dwFlags = FLASHW_STOP;
  }
  FlashWindowEx(&fwi);
}

void HWNDMessageHandler::ClearNativeFocus() {
  ::SetFocus(hwnd());
}

bool HWNDMessageHandler::SetTitle(const std::u16string& title) {
  std::wstring current_title;
  size_t len_with_null = GetWindowTextLength(hwnd()) + 1;
  if (len_with_null == 1 && title.length() == 0) {
    return false;
  }
  if (len_with_null - 1 == title.length() &&
      GetWindowText(hwnd(), base::WriteInto(&current_title, len_with_null),
                    len_with_null) &&
      current_title == base::AsWStringView(title)) {
    return false;
  }
  SetWindowText(hwnd(), base::as_wcstr(title));
  return true;
}

void HWNDMessageHandler::FrameTypeChanged() {
  needs_dwm_frame_clear_ = true;
  delegate_->HandleFrameChanged();
  UpdateDwmFrame();
}

void HWNDMessageHandler::SetFullscreen(bool fullscreen) {
  background_fullscreen_hack_ = false;
  auto ref = msg_handler_weak_factory_.GetWeakPtr();
  fullscreen_handler()->SetFullscreen(fullscreen);
  if (!ref) {
    return;
  }

  // Add the fullscreen window to the fullscreen window map which is used to
  // handle window activations.
  HMONITOR monitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY);
  if (fullscreen) {
    (fullscreen_monitor_map_.Get())[monitor] = this;
  } else {
    FullscreenWindowMonitorMap::iterator iter =
        fullscreen_monitor_map_.Get().find(monitor);
    if (iter != fullscreen_monitor_map_.Get().end()) {
      fullscreen_monitor_map_.Get().erase(iter);
    }
  }
}

void HWNDMessageHandler::SetAspectRatio(float aspect_ratio) {
  // If the aspect ratio is 0, reset it to null.
  if (aspect_ratio == 0.0f) {
    aspect_ratio_.reset();
    return;
  }

  aspect_ratio_ = aspect_ratio;

  // When the aspect ratio is set, size the window to adhere to it. This keeps
  // the same origin point as the original window.
  RECT window_rect;
  if (GetWindowRect(hwnd(), &window_rect)) {
    gfx::Rect rect(window_rect);

    SizeWindowToAspectRatio(WMSZ_BOTTOMRIGHT, &rect);
    SetBoundsInternal(rect, false);
  }
}

void HWNDMessageHandler::SizeConstraintsChanged() {
  LONG style = GetWindowLong(hwnd(), GWL_STYLE);
  // Ignore if this is not a standard window.
  if (style & (WS_POPUP | WS_CHILD)) {
    return;
  }

  LONG new_style = style;
  // Windows cannot have WS_THICKFRAME set if translucent.
  // See CalculateWindowStylesFromInitParams().
  if (delegate_->CanResize() && !is_translucent_) {
    new_style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    if (!delegate_->CanMaximize()) {
      new_style &= ~WS_MAXIMIZEBOX;
    }
  } else {
    new_style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }
  if (delegate_->CanMinimize()) {
    new_style |= WS_MINIMIZEBOX;
  } else {
    new_style &= ~WS_MINIMIZEBOX;
  }
  if (new_style == style) {
    return;
  }
  SetWindowLong(hwnd(), GWL_STYLE, new_style);
  SendFrameChanged();
}

LRESULT HWNDMessageHandler::OnWndProc(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param) {
  HWND window = hwnd();
  LRESULT result = 0;
  if (delegate_ &&
      delegate_->PreHandleMSG(message, w_param, l_param, &result)) {
    return result;
  }

  // Otherwise we handle everything else.
  // NOTE: We inline ProcessWindowMessage() as 'this' may be destroyed during
  // dispatch and ProcessWindowMessage() doesn't deal with that well.
  const BOOL old_msg_handled = msg_handled_;
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  const BOOL processed =
      _ProcessWindowMessage(window, message, w_param, l_param, result, 0);
  if (!ref) {
    return 0;
  }
  msg_handled_ = old_msg_handled;

  if (!processed) {
    result = DefWindowProc(window, message, w_param, l_param);
    // DefWindowProc() may have destroyed the window and/or us in a nested
    // message loop.
    if (!ref || !::IsWindow(window)) {
      return result;
    }
  }

  if (delegate_) {
    delegate_->PostHandleMSG(message, w_param, l_param);
    if (message == WM_NCDESTROY) {
      RestoreEnabledIfNecessary();
      delegate_->HandleDestroyed();
    }
  }

  if (message == WM_ACTIVATE && IsTopLevelWindow(window)) {
    PostProcessActivateMessage(LOWORD(w_param), !!HIWORD(w_param),
                               reinterpret_cast<HWND>(l_param));
  }
  return result;
}

int HWNDMessageHandler::GetAppbarAutohideEdges(HMONITOR monitor) {
  int edges = 0;
  constexpr UINT kAppbarEdges[] = {ABE_LEFT, ABE_TOP, ABE_RIGHT, ABE_BOTTOM};
  for (UINT edge : kAppbarEdges) {
    APPBARDATA abd = {};
    abd.cbSize = sizeof(abd);
    abd.uEdge = edge;
#if defined(ABM_GETAUTOHIDEBAREX)
    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!::GetMonitorInfo(monitor, &monitor_info)) {
      continue;
    }
    abd.rc = monitor_info.rcMonitor;
    const UINT message = ABM_GETAUTOHIDEBAREX;
#else
    const UINT message = ABM_GETAUTOHIDEBAR;
#endif
    const HWND auto_hide_bar =
        reinterpret_cast<HWND>(::SHAppBarMessage(message, &abd));
    if (!auto_hide_bar) {
      continue;
    }
    if (::MonitorFromWindow(auto_hide_bar, MONITOR_DEFAULTTONULL) != monitor) {
      continue;
    }
    edges |= AutoHideEdgeMaskFromAppbarEdge(edge);
  }
  return edges;
}

void HWNDMessageHandler::OnAppbarAutohideEdgesChanged() {
  // This triggers querying WM_NCCALCSIZE again.
  RECT client;
  GetWindowRect(hwnd(), &client);
  SetWindowPos(hwnd(), nullptr, client.left, client.top,
               client.right - client.left, client.bottom - client.top,
               SWP_FRAMECHANGED);
}

void HWNDMessageHandler::SetInitialFocus() {
  if (!(GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_TRANSPARENT) &&
      !(GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_NOACTIVATE)) {
    // The window does not get keyboard messages unless we focus it.
    SetFocus(hwnd());
  }
}

void HWNDMessageHandler::PostProcessActivateMessage(
    int activation_state,
    bool minimized,
    HWND window_gaining_or_losing_activation) {
  DCHECK(IsTopLevelWindow(hwnd()));
  const bool active = activation_state != WA_INACTIVE && !minimized;
  if (notify_restore_on_activate_) {
    notify_restore_on_activate_ = false;
    delegate_->HandleWindowMinimizedOrRestored(/*restored=*/true);
    // Prevent OnSize() from also calling HandleWindowMinimizedOrRestored.
    last_size_param_ = SIZE_RESTORED;
  }
  if (delegate_->CanActivate()) {
    delegate_->HandleActivationChanged(active);
  }

  if (!::IsWindow(window_gaining_or_losing_activation)) {
    window_gaining_or_losing_activation = ::GetForegroundWindow();
  }

  // If the window losing activation is a fullscreen window, we reduce the size
  // of the window by 1px. i.e. Not fullscreen. This is to work around an
  // apparent bug in the Windows taskbar where in it tracks fullscreen state on
  // a per thread basis. This causes it not be a topmost window when any
  // maximized window on a thread which has a fullscreen window is active. This
  // affects the way these windows interact with the taskbar, they obscure it
  // when maximized, autohide does not work correctly, etc.
  // By reducing the size of the fullscreen window by 1px, we ensure that the
  // taskbar no longer treats the window and in turn the thread as a fullscreen
  // thread. This in turn ensures that maximized windows on the same thread
  // don't obscure the taskbar, etc.
  // Please note that this taskbar behavior only occurs if the window becoming
  // active is on the same monitor as the fullscreen window.
  if (!active) {
    if (IsFullscreen() && ::IsWindow(window_gaining_or_losing_activation)) {
      HMONITOR active_window_monitor = MonitorFromWindow(
          window_gaining_or_losing_activation, MONITOR_DEFAULTTOPRIMARY);
      HMONITOR fullscreen_window_monitor =
          MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY);

      if (active_window_monitor == fullscreen_window_monitor) {
        OnBackgroundFullscreen();
      }
    }
  } else if (background_fullscreen_hack_) {
    // Restore the bounds of the window to fullscreen.
    DCHECK(IsFullscreen());
    MONITORINFO monitor_info = {sizeof(monitor_info)};
    GetMonitorInfo(MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY),
                   &monitor_info);
    SetBoundsInternal(gfx::Rect(monitor_info.rcMonitor), false);
    // Inform the taskbar that this window is now a fullscreen window so it go
    // behind the window in the Z-Order. The taskbar heuristics to detect
    // fullscreen windows are not reliable. Marking it explicitly seems to work
    // around these problems.
    fullscreen_handler()->MarkFullscreen(true);
    background_fullscreen_hack_ = false;
  } else {
    // If the window becoming active has a fullscreen window on the same
    // monitor then we need to reduce the size of the fullscreen window by
    // 1 px. Please refer to the comments above for the reasoning behind
    // this.
    CheckAndHandleBackgroundFullscreenOnMonitor(
        window_gaining_or_losing_activation);
  }
}

void HWNDMessageHandler::RestoreEnabledIfNecessary() {
  if (delegate_->IsModal() && !restored_enabled_) {
    restored_enabled_ = true;
    // If we were run modally, we need to undo the disabled-ness we inflicted on
    // the owner's parent hierarchy.
    HWND start = ::GetWindow(hwnd(), GW_OWNER);
    while (start) {
      ::EnableWindow(start, TRUE);
      start = ::GetParent(start);
    }
  }
}

void HWNDMessageHandler::ExecuteSystemMenuCommand(int command) {
  if (command) {
    SendMessage(hwnd(), WM_SYSCOMMAND, command, 0);
  }
}

void HWNDMessageHandler::ClientAreaSizeChanged() {
  // Ignore size changes due to fullscreen windows losing activation.
  if (background_fullscreen_hack_ && !sent_window_size_changing_) {
    return;
  }
  auto ref = msg_handler_weak_factory_.GetWeakPtr();
  delegate_->HandleClientSizeChanged(GetClientAreaBounds().size());
  if (!ref) {
    return;
  }

  current_window_size_message_++;
  sent_window_size_changing_ = false;
}

bool HWNDMessageHandler::GetClientAreaInsets(gfx::Insets* insets,
                                             HMONITOR monitor) const {
  if (delegate_->GetClientAreaInsets(insets, monitor)) {
    return true;
  }
  DCHECK(insets->IsEmpty());

  if (HasSystemFrame()) {
    return false;
  }

  if (IsMaximized()) {
    // Windows automatically adds a standard width border to all sides when a
    // window is maximized.
    int frame_thickness = GetFrameThickness(monitor);
    if (!delegate_->HasFrame()) {
      frame_thickness -= 1;
    }
    *insets = gfx::Insets(frame_thickness);
    return true;
  }

  *insets = gfx::Insets();
  return true;
}

LRESULT HWNDMessageHandler::DefWindowProcWithRedrawLock(UINT message,
                                                        WPARAM w_param,
                                                        LPARAM l_param) {
  LRESULT result = DefWindowProc(hwnd(), message, w_param, l_param);
  return result;
}

bool HWNDMessageHandler::IsFrameSystemDrawn() const {
  FrameMode frame_mode = delegate_->GetFrameMode();
  return frame_mode == FrameMode::SYSTEM_DRAWN;
}

bool HWNDMessageHandler::HasSystemFrame() const {
  return delegate_->HasFrame() && IsFrameSystemDrawn();
}

// Message handlers ------------------------------------------------------------

void HWNDMessageHandler::OnActivateApp(BOOL active, DWORD thread_id) {
  if (!active && thread_id != GetCurrentThreadId()) {
    // Update the native frame if it is rendering the non-client area.
    if (HasSystemFrame()) {
      DefWindowProcWithRedrawLock(WM_NCACTIVATE, FALSE, 0);
    }
  }
}

BOOL HWNDMessageHandler::OnAppCommand(HWND window,
                                      int command,
                                      WORD device,
                                      WORD keystate) {
  BOOL handled = !!delegate_->HandleAppCommand(command);
  SetMsgHandled(handled);
  // Make sure to return TRUE if the event was handled or in some cases the
  // system will execute the default handler which can cause bugs like going
  // forward or back two pages instead of one.
  return handled;
}

void HWNDMessageHandler::OnCancelMode() {
  delegate_->HandleCancelMode();
  // Need default handling, otherwise capture and other things aren't canceled.
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnClose() {
  delegate_->HandleClose();
}

void HWNDMessageHandler::OnCommand(UINT notification_code,
                                   int command,
                                   HWND window) {
  // If the notification code is > 1 it means it is control specific and we
  // should ignore it.
  if (notification_code > 1) {
    SetMsgHandled(FALSE);
    return;
  }
  SetMsgHandled(delegate_->HandleCommand(command));
}

LRESULT HWNDMessageHandler::OnCreate(CREATESTRUCT* create_struct) {
  if (is_translucent_) {
    // This is part of the magic to emulate layered windows with Aura
    // see the explanation elsewere when we set is_translucent_.
    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd(), &margins);

    ::SetProp(hwnd(), ui::kWindowTranslucent, reinterpret_cast<HANDLE>(1));
  }

  fullscreen_handler_->set_hwnd(hwnd());

  // This message initializes the window so that focus border are shown for
  // windows.
  SendMessage(hwnd(), WM_CHANGEUISTATE, MAKELPARAM(UIS_CLEAR, UISF_HIDEFOCUS),
              0);

  if (!delegate_->HasFrame()) {
    LONG style = GetWindowLong(hwnd(), GWL_STYLE);
    if (style & WS_CAPTION) {
      SetWindowLong(hwnd(), GWL_STYLE, style & ~WS_CAPTION);
      SendFrameChanged();
    }
  }

  // Get access to a modifiable copy of the system menu.
  GetSystemMenu(hwnd(), false);

  if (!pointer_events_for_touch_) {
    RegisterTouchWindow(hwnd(), TWF_WANTPALM);
  }

  // We need to allow the delegate to size its contents since the window may not
  // receive a size notification when its initial bounds are specified at window
  // creation time.
  ClientAreaSizeChanged();

  delegate_->HandleCreate();

  // session_change_observer_ =
  //     std::make_unique<ui::SessionChangeObserver>(base::BindRepeating(
  //         &HWNDMessageHandler::OnSessionChange, base::Unretained(this)));

  dpi_ = lynxtron::GetDPIForHWND(hwnd());
  return 0;
}

void HWNDMessageHandler::OnDestroy() {
  ::RemoveProp(hwnd(), ui::kWindowTranslucent);
  // session_change_observer_.reset(nullptr);
  delegate_->HandleDestroying();
  // If the window going away is a fullscreen window then remove its references
  // from the full screen window map.
  auto& map = fullscreen_monitor_map_.Get();
  const auto i = std::find_if(map.begin(), map.end(), [this](const auto& elem) {
    return elem.second == this;
  });
  if (i != map.end()) {
    map.erase(i);
  }

  // If we have ever returned a UIA object via WM_GETOBJECT, signal that all
  // objects associated with this HWND can be discarded. See:
  // https://docs.microsoft.com/en-us/windows/win32/api/uiautomationcoreapi/nf-uiautomationcoreapi-uiareturnrawelementprovider#remarks
  // if (did_return_uia_object_)
  //  UiaReturnRawElementProvider(hwnd(), 0, 0, nullptr);
}

void HWNDMessageHandler::OnDisplayChange(UINT bits_per_pixel,
                                         const gfx::Size& screen_size) {
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  delegate_->HandleDisplayChange();

  // HandleDisplayChange() may result in |this| being deleted.
  if (!ref) {
    return;
  }

  // Force a WM_NCCALCSIZE to occur to ensure that we handle auto hide
  // taskbars correctly.
  SendFrameChanged();
}

LRESULT HWNDMessageHandler::OnDwmCompositionChanged(UINT msg,
                                                    WPARAM /* w_param */,
                                                    LPARAM /* l_param */) {
  // if (!delegate_->HasNonClientView()) {
  //   SetMsgHandled(FALSE);
  //   return 0;
  // }

  bool dwm_composition_enabled = ui::win::IsDwmCompositionEnabled();
  if (dwm_composition_enabled_ != dwm_composition_enabled) {
    // Do not cause the Window to be hidden and shown unless there was
    // an actual change in the theme. This filter is necessary because
    // Windows sends redundant WM_DWMCOMPOSITIONCHANGED messages when
    // a laptop is reopened, and our theme change code causes wonky
    // focus issues. See http://crbug.com/895855 for more information.
    dwm_composition_enabled_ = dwm_composition_enabled;
    FrameTypeChanged();
  }
  return 0;
}

LRESULT HWNDMessageHandler::OnDpiChanged(UINT msg,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  if (LOWORD(w_param) != HIWORD(w_param)) {
    // NOTIMPLEMENTED() << "Received non-square scaling factors";
  }

  dpi_ = LOWORD(w_param);
  float scaling_factor = lynxtron::GetScalingFactorFromDPI(dpi_);

  SetBoundsInternal(gfx::Rect(*reinterpret_cast<RECT*>(l_param)), false);
  delegate_->HandleWindowScaleFactorChanged(scaling_factor);
  return 0;
}

void HWNDMessageHandler::OnEnterMenuLoop(BOOL from_track_popup_menu) {
  if (menu_depth_++ == 0) {
    delegate_->HandleMenuLoop(true);
  }
}

void HWNDMessageHandler::OnEnterSizeMove() {
  delegate_->HandleBeginWMSizeMove();
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnEraseBkgnd(HDC dc) {
  gfx::Insets insets;
  if (ui::win::IsAeroGlassEnabled() &&
      delegate_->GetDwmFrameInsetsInPixels(&insets) && !insets.IsEmpty() &&
      needs_dwm_frame_clear_) {
    // This is necessary to avoid white flashing in the titlebar area around the
    // minimize/maximize/close buttons.
    needs_dwm_frame_clear_ = false;
    RECT client_rect;
    GetClientRect(hwnd(), &client_rect);
    base::win::ScopedGDIObject<HBRUSH> brush(CreateSolidBrush(0));
    // The DC and GetClientRect operate in client area coordinates.
    RECT rect = {0, 0, client_rect.right, insets.top()};
    FillRect(dc, &rect, brush.get());
  }
  // Needed to prevent resize flicker.
  return 1;
}

void HWNDMessageHandler::OnExitMenuLoop(BOOL is_shortcut_menu) {
  if (--menu_depth_ == 0) {
    delegate_->HandleMenuLoop(false);
  }
  DCHECK_GE(0, menu_depth_);
}

void HWNDMessageHandler::OnExitSizeMove() {
  delegate_->HandleEndWMSizeMove();
  SetMsgHandled(FALSE);
  // If the window was moved to a monitor which has a fullscreen window active,
  // we need to reduce the size of the fullscreen window by 1px.
  CheckAndHandleBackgroundFullscreenOnMonitor(hwnd());
}

void HWNDMessageHandler::OnGetMinMaxInfo(MINMAXINFO* minmax_info) {
  gfx::Size min_window_size;
  gfx::Size max_window_size;
  delegate_->GetMinMaxSize(&min_window_size, &max_window_size);
  min_window_size = delegate_->DIPToScreenSize(min_window_size);
  max_window_size = delegate_->DIPToScreenSize(max_window_size);

  // Add the native frame border size to the minimum and maximum size if the
  // view reports its size as the client size.
  if (HasSystemFrame()) {
    const gfx::Size non_client_size = GetNonClientSizeInPixels(hwnd());
    min_window_size.Enlarge(non_client_size.width(), non_client_size.height());
    if (max_window_size.width()) {
      max_window_size.Enlarge(non_client_size.width(), 0);
    }
    if (max_window_size.height()) {
      max_window_size.Enlarge(0, non_client_size.height());
    }
  }
  minmax_info->ptMinTrackSize.x = min_window_size.width();
  minmax_info->ptMinTrackSize.y = min_window_size.height();
  if (max_window_size.width() || max_window_size.height()) {
    if (!max_window_size.width()) {
      max_window_size.set_width(GetSystemMetrics(SM_CXMAXTRACK));
    }
    if (!max_window_size.height()) {
      max_window_size.set_height(GetSystemMetrics(SM_CYMAXTRACK));
    }
    minmax_info->ptMaxTrackSize.x = max_window_size.width();
    minmax_info->ptMaxTrackSize.y = max_window_size.height();
  }
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnInitMenu(HMENU menu) {
  bool is_fullscreen = IsFullscreen();
  bool is_minimized = IsMinimized();
  bool is_maximized = IsMaximized();
  bool is_restored = !is_fullscreen && !is_minimized && !is_maximized;

  EnableMenuItemByCommand(
      menu, SC_RESTORE,
      delegate_->CanResize() && (is_minimized || is_maximized));
  EnableMenuItemByCommand(menu, SC_MOVE, is_restored);
  EnableMenuItemByCommand(menu, SC_SIZE, delegate_->CanResize() && is_restored);
  EnableMenuItemByCommand(
      menu, SC_MAXIMIZE,
      delegate_->CanMaximize() && !is_fullscreen && !is_maximized);
  EnableMenuItemByCommand(menu, SC_MINIMIZE,
                          delegate_->CanMinimize() && !is_minimized);

  if (is_maximized && delegate_->CanResize()) {
    ::SetMenuDefaultItem(menu, SC_RESTORE, FALSE);
  } else if (!is_maximized && delegate_->CanMaximize()) {
    ::SetMenuDefaultItem(menu, SC_MAXIMIZE, FALSE);
  }
}

void HWNDMessageHandler::OnKillFocus(HWND focused_window) {
  delegate_->HandleNativeBlur(focused_window);
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnMove(const gfx::Point& point) {
  delegate_->HandleMove();
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnMoving(UINT param, RECT* new_bounds) {
  delegate_->HandleMoving(new_bounds);
}

LRESULT HWNDMessageHandler::OnNCActivate(UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  if (HasSystemFrame()) {
    SetMsgHandled(FALSE);
    return 0;
  }
  // For custom-drawn (frameless) windows, allow activation state changes
  // without letting User32 paint the default non-client area. Otherwise Windows
  // may draw an inactive border when the window loses focus.
  return TRUE;
}

LRESULT HWNDMessageHandler::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  if (HasSystemFrame()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  // Let User32 handle the first NCCALCSIZE for captioned windows so it updates
  // its internal structures (specifically caption-present). This is a legacy
  // workaround (Tile/Cascade depends on it). Lynxtron usually creates
  // non-system-frame windows without WS_CAPTION, so this typically only applies
  // during transitional style states.
  // See http://code.google.com/p/chromium/issues/detail?id=900
  if (is_first_nccalc_) {
    is_first_nccalc_ = false;
    if (GetWindowLong(hwnd(), GWL_STYLE) & WS_CAPTION) {
      SetMsgHandled(FALSE);
      return 0;
    }
  }

  RECT* client_rect =
      mode ? &(reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param)->rgrc[0])
           : reinterpret_cast<RECT*>(l_param);

  HMONITOR monitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONULL);
  if (!monitor) {
    // We might end up here if the window was previously minimized and the
    // user clicks on the taskbar button to restore it in the previous
    // position. In that case WM_NCCALCSIZE is sent before the window
    // coordinates are restored to their previous values, so our (left,top)
    // would probably be (-32000,-32000) like all minimized windows. So the
    // above MonitorFromWindow call fails, but if we check the window rect
    // given with WM_NCCALCSIZE (which is our previous restored window
    // position) we will get the correct monitor handle.
    monitor = MonitorFromRect(client_rect, MONITOR_DEFAULTTONULL);
    if (!monitor) {
      // This is probably an extreme case that we won't hit, but if we don't
      // intersect any monitor, let us not adjust the client rect since our
      // window will not be visible anyway.
      return 0;
    }
  }

  gfx::Insets insets;
  bool got_insets = GetClientAreaInsets(&insets, monitor);
  if (!got_insets && !IsFullscreen()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  client_rect->left += insets.left();
  client_rect->top += insets.top();
  client_rect->bottom -= insets.bottom();
  client_rect->right -= insets.right();
  if (IsMaximized()) {
    // Find all auto-hide taskbars along the screen edges and adjust in by the
    // thickness of the auto-hide taskbar on each such edge, so the window isn't
    // treated as a "fullscreen app", which would cause the taskbars to
    // disappear.
    const int autohide_edges = GetAppbarAutohideEdges(monitor);
    if (autohide_edges & kAutoHideEdgeLeft) {
      client_rect->left += kAutoHideTaskbarThicknessPx;
    }
    if (autohide_edges & kAutoHideEdgeTop) {
      client_rect->top += kAutoHideTaskbarThicknessPx;
    }
    if (autohide_edges & kAutoHideEdgeRight) {
      client_rect->right -= kAutoHideTaskbarThicknessPx;
    }
    if (autohide_edges & kAutoHideEdgeBottom) {
      client_rect->bottom -= kAutoHideTaskbarThicknessPx;
    }

    // We cannot return WVR_REDRAW when there is nonclient area, or Windows
    // exhibits bugs where client pixels and child HWNDs are mispositioned by
    // the width/height of the upper-left nonclient area.
    return 0;
  }

  // If the window bounds change, we're going to relayout and repaint anyway.
  // Returning WVR_REDRAW avoids an extra paint before that of the old client
  // pixels in the (now wrong) location, and thus makes actions like resizing a
  // window from the left edge look slightly less broken.
  // We special case when left or top insets are 0, since these conditions
  // actually require another repaint to correct the layout after glass gets
  // turned on and off.
  if (insets.left() == 0 || insets.top() == 0) {
    return 0;
  }
  return mode ? WVR_REDRAW : 0;
}

LRESULT HWNDMessageHandler::OnNCCreate(LPCREATESTRUCT lpCreateStruct) {
  SetMsgHandled(FALSE);
  if (!delegate_->HasFrame()) {
    // Ensure frameless windows never enter the captioned-window non-client
    // initialization path.
    LONG style = GetWindowLong(hwnd(), GWL_STYLE);
    if (style & WS_CAPTION) {
      SetWindowLong(hwnd(), GWL_STYLE, style & ~WS_CAPTION);
    }
  }
  if (delegate_->HasFrame() && lynxtron::IsProcessPerMonitorDpiAware()) {
    using EnableNonClientDpiScalingPtr = decltype(::EnableNonClientDpiScaling)*;
    static const auto enable_non_client_dpi_scaling_func =
        reinterpret_cast<EnableNonClientDpiScalingPtr>(
            base::win::GetUser32FunctionPointer("EnableNonClientDpiScaling"));
    called_enable_non_client_dpi_scaling_ =
        !!(enable_non_client_dpi_scaling_func &&
           enable_non_client_dpi_scaling_func(hwnd()));
  }
  return FALSE;
}

LRESULT HWNDMessageHandler::OnNCHitTest(const gfx::Point& point) {
  if (HasSystemFrame()) {
    LRESULT result;
    if (DwmDefWindowProc(hwnd(), WM_NCHITTEST, 0,
                         MAKELPARAM(point.x(), point.y()), &result)) {
      return result;
    }
  }
  if (!HasSystemFrame() && delegate_->CanResize() && !IsMaximized() &&
      !IsFullscreen()) {
    // WM_NCCALCSIZE makes the whole window area client for frameless windows,
    // so we must provide resize hit testing manually.
    RECT window_rect{};
    if (GetWindowRect(hwnd(), &window_rect)) {
      HMONITOR monitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONEAREST);
      const int frame_thickness = GetFrameThickness(monitor);
      const int x = point.x();
      const int y = point.y();

      const bool left = x < (window_rect.left + frame_thickness);
      const bool right = x >= (window_rect.right - frame_thickness);
      const bool top = y < (window_rect.top + frame_thickness);
      const bool bottom = y >= (window_rect.bottom - frame_thickness);

      if (top && left) {
        return HTTOPLEFT;
      }
      if (top && right) {
        return HTTOPRIGHT;
      }
      if (bottom && left) {
        return HTBOTTOMLEFT;
      }
      if (bottom && right) {
        return HTBOTTOMRIGHT;
      }
      if (left) {
        return HTLEFT;
      }
      if (right) {
        return HTRIGHT;
      }
      if (top) {
        return HTTOP;
      }
      if (bottom) {
        return HTBOTTOM;
      }
    }
  }
  return DefWindowProc(hwnd(), WM_NCHITTEST, 0,
                       MAKELPARAM(point.x(), point.y()));
}

LRESULT HWNDMessageHandler::OnNCPaint(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param) {
  if (HasSystemFrame()) {
    SetMsgHandled(FALSE);
    return 0;
  }
  // Prevent default non-client painting for custom-drawn (frameless) windows to
  // avoid system-drawn borders/shadows appearing during activation changes.
  return 0;
}

LRESULT HWNDMessageHandler::OnNCUAHDrawCaption(UINT message,
                                               WPARAM w_param,
                                               LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN);
  return 0;
}

LRESULT HWNDMessageHandler::OnNCUAHDrawFrame(UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN);
  return 0;
}

void HWNDMessageHandler::OnPaint(HDC dc) {
  // Call BeginPaint()/EndPaint() around the paint handling, as that seems
  // to do more to actually validate the window's drawing region. This only
  // appears to matter for Windows that have the WS_EX_COMPOSITED style set
  // but will be valid in general too.
  PAINTSTRUCT ps;
  HDC display_dc = BeginPaint(hwnd(), &ps);

  if (!display_dc) {
    // Failing to get a DC during BeginPaint() means we won't be able to
    // actually get any pixels to the screen and is very bad. This is often
    // caused by handle exhaustion. Collecting some GDI statistics may aid
    // tracking down the cause.
    base::debug::CollectGDIUsageAndDie();
  }

  if (!IsRectEmpty(&ps.rcPaint)) {
    HBRUSH brush =
        reinterpret_cast<HBRUSH>(GetClassLongPtr(hwnd(), GCLP_HBRBACKGROUND));
    if (!brush) {
      brush = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    }

    if (exposed_pixels_ != gfx::Size()) {
      // Fill in newly exposed window client area with black to ensure Windows
      // doesn't put something else there (eg. copying existing pixels). This
      // isn't needed if we've just cleared the whole client area outside the
      // child window above.
      RECT cr;
      if (GetClientRect(hwnd(), &cr)) {
        // GetClientRect() always returns a rect with top/left at 0.
        const gfx::Size client_area = gfx::Rect(cr).size();

        // It's possible that |exposed_pixels_| height and/or width is larger
        // than the client area if the window frame size changed. This isn't an
        // issue since FillRect() is clipped by |ps.rcPaint|.
        if (exposed_pixels_.height() > 0) {
          RECT rect = {0, client_area.height() - exposed_pixels_.height(),
                       client_area.width(), client_area.height()};
          FillRect(ps.hdc, &rect, brush);
        }
        if (exposed_pixels_.width() > 0) {
          RECT rect = {client_area.width() - exposed_pixels_.width(), 0,
                       client_area.width(),
                       client_area.height() - exposed_pixels_.height()};
          FillRect(ps.hdc, &rect, brush);
        }
      }
    }
  }

  exposed_pixels_ = gfx::Size();

  EndPaint(hwnd(), &ps);
}

void HWNDMessageHandler::OnSetFocus(HWND last_focused_window) {
  delegate_->HandleNativeFocus(last_focused_window);
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnSetIcon(UINT size_type, HICON new_icon) {
  return DefWindowProcWithRedrawLock(WM_SETICON, size_type,
                                     reinterpret_cast<LPARAM>(new_icon));
}

LRESULT HWNDMessageHandler::OnSetText(const wchar_t* text) {
  return DefWindowProcWithRedrawLock(WM_SETTEXT, NULL,
                                     reinterpret_cast<LPARAM>(text));
}

void HWNDMessageHandler::OnSettingChange(UINT flags, const wchar_t* section) {
  if (!GetParent(hwnd()) && (flags == SPI_SETWORKAREA)) {
    // Fire a dummy SetWindowPos() call, so we'll trip the code in
    // OnWindowPosChanging() below that notices work area changes.
    ::SetWindowPos(hwnd(), nullptr, 0, 0, 0, 0,
                   SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW |
                       SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    SetMsgHandled(TRUE);
  } else {
    if (flags == SPI_SETWORKAREA) {
      delegate_->HandleWorkAreaChanged();
    }
    SetMsgHandled(FALSE);
  }

  // If the work area is changing, then it could be as a result of the taskbar
  // broadcasting the WM_SETTINGCHANGE message due to changes in auto hide
  // settings, etc. Force a WM_NCCALCSIZE to occur to ensure that we handle
  // this correctly.
  if (flags == SPI_SETWORKAREA) {
    SendFrameChanged();
  }
}

bool DidMinimizedChange(UINT old_size_param, UINT new_size_param) {
  return (
      (old_size_param == SIZE_MINIMIZED && new_size_param != SIZE_MINIMIZED) ||
      (old_size_param != SIZE_MINIMIZED && new_size_param == SIZE_MINIMIZED));
}

bool DidMaximizedChange(UINT old_size_param, UINT new_size_param) {
  return (
      (old_size_param == SIZE_MAXIMIZED && new_size_param != SIZE_MAXIMIZED) ||
      (old_size_param != SIZE_MAXIMIZED && new_size_param == SIZE_MAXIMIZED));
}

void HWNDMessageHandler::OnSize(UINT param, const gfx::Size& size) {
  if (IsTopLevelWindow(hwnd())) {
    if (DidMinimizedChange(last_size_param_, param)) {
      delegate_->HandleWindowMinimizedOrRestored(param != SIZE_MINIMIZED);
    }
    if (DidMaximizedChange(last_size_param_, param)) {
      delegate_->HandleWindowMaximized(param == SIZE_MAXIMIZED);
    }
  }
  last_size_param_ = param;

  RedrawWindow(hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void HWNDMessageHandler::OnSizing(UINT param, RECT* rect) {
  // If the aspect ratio was not specified for the window, do nothing.
  if (!aspect_ratio_.has_value()) {
    return;
  }

  gfx::Rect window_rect(*rect);
  SizeWindowToAspectRatio(param, &window_rect);

  // TODO(apacible): Account for window borders as part of the aspect ratio.
  // https://crbug/869487.
  *rect = window_rect.ToRECT();
}

void HWNDMessageHandler::OnSysCommand(UINT notification_code,
                                      const gfx::Point& point) {
  // Windows uses the 4 lower order bits of |notification_code| for type-
  // specific information so we must exclude this when comparing.
  static const int sc_mask = 0xFFF0;
  // Ignore size/move/maximize in fullscreen mode.
  if (IsFullscreen() && (((notification_code & sc_mask) == SC_SIZE) ||
                         ((notification_code & sc_mask) == SC_MOVE) ||
                         ((notification_code & sc_mask) == SC_MAXIMIZE))) {
    return;
  }

  const bool window_control_action =
      (notification_code & sc_mask) == SC_MINIMIZE ||
      (notification_code & sc_mask) == SC_MAXIMIZE ||
      (notification_code & sc_mask) == SC_RESTORE;
  const bool custom_controls_frame_mode =

      delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN;
  if (custom_controls_frame_mode && window_control_action) {
    delegate_->ResetWindowControls();
  }

  if (delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN) {
    const bool window_bounds_change =
        (notification_code & sc_mask) == SC_MOVE ||
        (notification_code & sc_mask) == SC_SIZE;
    if (window_bounds_change && !IsVisible()) {
      // Circumvent ScopedRedrawLocks and force visibility before entering a
      // resize or move modal loop to get continuous sizing/moving feedback.
      SetWindowLong(hwnd(), GWL_STYLE,
                    GetWindowLong(hwnd(), GWL_STYLE) | WS_VISIBLE);
    }
  }

  if (delegate_->HandleCommand(notification_code)) {
    return;
  }

  bool is_mouse_menu = (notification_code & sc_mask) == SC_MOUSEMENU;
  if (is_mouse_menu) {
    handling_mouse_menu_ = true;
  }

  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  // If the delegate can't handle it, the system implementation will be called.
  DefWindowProc(hwnd(), WM_SYSCOMMAND, notification_code,
                MAKELPARAM(point.x(), point.y()));
  if (is_mouse_menu && ref) {
    handling_mouse_menu_ = false;
  }
}

void HWNDMessageHandler::OnThemeChanged() {}

void HWNDMessageHandler::OnTimeChange() {
  // Call NowFromSystemTime() to force base::Time to re-initialize the clock
  // from system time. Otherwise base::Time::Now() might continue to reflect the
  // old system clock for some amount of time. See https://crbug.com/672906#c5
  base::Time::NowFromSystemTime();
}

void HWNDMessageHandler::OnWindowPosChanging(WINDOWPOS* window_pos) {
  if (ignore_window_pos_changes_) {
    // If somebody's trying to toggle our visibility, change the nonclient area,
    // change our Z-order, or activate us, we should probably let it go through.
    if (!(window_pos->flags & ((IsVisible() ? SWP_HIDEWINDOW : SWP_SHOWWINDOW) |
                               SWP_FRAMECHANGED)) &&
        (window_pos->flags & (SWP_NOZORDER | SWP_NOACTIVATE))) {
      // Just sizing/moving the window; ignore.
      window_pos->flags |= SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW;
      window_pos->flags &= ~(SWP_SHOWWINDOW | SWP_HIDEWINDOW);
    }
  } else if (!GetParent(hwnd())) {
    RECT window_rect;
    const bool have_new_window_rect =
        !(window_pos->flags & SWP_NOMOVE) && !(window_pos->flags & SWP_NOSIZE);
    if (have_new_window_rect) {
      // We should use new window rect for detecting monitor and it's
      // parameters, if it is available. If we use |GetWindowRect()| instead,
      // we can break our same monitor detection logic (see |same_monitor|
      // below) and consequently Windows "Move to other monitor" shortcuts
      // (Win+Shift+Arrows). See crbug.com/656001.
      window_rect.left = window_pos->x;
      window_rect.top = window_pos->y;
      window_rect.right = window_pos->x + window_pos->cx;
      window_rect.bottom = window_pos->y + window_pos->cy;
    }

    HMONITOR monitor;
    gfx::Rect monitor_rect, work_area;
    if ((have_new_window_rect || GetWindowRect(hwnd(), &window_rect)) &&
        GetMonitorAndRects(window_rect, &monitor, &monitor_rect, &work_area)) {
      bool work_area_changed = (monitor_rect == last_monitor_rect_) &&
                               (work_area != last_work_area_);
      const bool same_monitor = monitor && (monitor == last_monitor_);

      gfx::Rect expected_maximized_bounds = work_area;
      if (IsMaximized()) {
        // Windows automatically adds a standard width border to all sides when
        // window is maximized. We should take this into account.
        gfx::Insets client_area_insets;
        if (GetClientAreaInsets(&client_area_insets, monitor)) {
          // Ceil the insets after scaling to make them exclude fractional parts
          // after scaling, since the result is negative.
          expected_maximized_bounds.Inset(
              gfx::ScaleToCeiledInsets(client_area_insets, -1));
        }
      }
      // Sometimes Windows incorrectly changes bounds of maximized windows after
      // attaching or detaching additional displays. In this case user can see
      // non-client area of the window (that should be hidden in normal case).
      // We should restore window position if problem occurs.
      const bool incorrect_maximized_bounds =
          IsMaximized() && have_new_window_rect &&
          (expected_maximized_bounds.x() != window_pos->x ||
           expected_maximized_bounds.y() != window_pos->y ||
           expected_maximized_bounds.width() != window_pos->cx ||
           expected_maximized_bounds.height() != window_pos->cy);

      // If the size of a background fullscreen window changes again, then we
      // should reset the |background_fullscreen_hack_| flag.
      if (background_fullscreen_hack_ &&
          (!(window_pos->flags & SWP_NOSIZE) &&
           (monitor_rect.height() - window_pos->cy != 1))) {
        background_fullscreen_hack_ = false;
      }
      const bool fullscreen_without_hack =
          IsFullscreen() && !background_fullscreen_hack_;

      if (same_monitor && (incorrect_maximized_bounds ||
                           fullscreen_without_hack || work_area_changed)) {
        // A rect for the monitor we're on changed.  Normally Windows notifies
        // us about this (and thus we're reaching here due to the SetWindowPos()
        // call in OnSettingChange() above), but with some software (e.g.
        // nVidia's nView desktop manager) the work area can change asynchronous
        // to any notification, and we're just sent a SetWindowPos() call with a
        // new (frequently incorrect) position/size.  In either case, the best
        // response is to throw away the existing position/size information in
        // |window_pos| and recalculate it based on the new work rect.
        gfx::Rect new_window_rect;
        if (IsFullscreen()) {
          new_window_rect = monitor_rect;
        } else if (IsMaximized()) {
          new_window_rect = expected_maximized_bounds;
        } else {
          new_window_rect = gfx::Rect(window_rect);
          new_window_rect.AdjustToFit(work_area);
        }
        window_pos->x = new_window_rect.x();
        window_pos->y = new_window_rect.y();
        window_pos->cx = new_window_rect.width();
        window_pos->cy = new_window_rect.height();
        // WARNING!  Don't set SWP_FRAMECHANGED here, it breaks moving the child
        // HWNDs for some reason.
        window_pos->flags &= ~(SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW);
        window_pos->flags |= SWP_NOCOPYBITS;

        // Now ignore all immediately-following SetWindowPos() changes.  Windows
        // likes to (incorrectly) recalculate what our position/size should be
        // and send us further updates.
        ignore_window_pos_changes_ = true;
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(&HWNDMessageHandler::StopIgnoringPosChanges,
                           msg_handler_weak_factory_.GetWeakPtr()));
      }
      last_monitor_ = monitor;
      last_monitor_rect_ = monitor_rect;
      last_work_area_ = work_area;
    }
  }

  RECT window_rect;
  gfx::Size old_size;
  if (GetWindowRect(hwnd(), &window_rect)) {
    old_size = gfx::Rect(window_rect).size();
  }
  gfx::Size new_size = gfx::Size(window_pos->cx, window_pos->cy);
  if ((old_size != new_size && !(window_pos->flags & SWP_NOSIZE)) ||
      window_pos->flags & SWP_FRAMECHANGED) {
    // If the window is getting larger then fill the exposed area on the next
    // WM_PAINT.
    exposed_pixels_ = new_size - old_size;

    delegate_->HandleWindowSizeChanging();
    sent_window_size_changing_ = true;

    // It's possible that if Aero snap is being entered then the window size
    // won't actually change. Post a message to ensure swaps will be re-enabled
    // in that case.
    PostMessage(hwnd(), WM_WINDOWSIZINGFINISHED, ++current_window_size_message_,
                0);
    // Copying the old bits can sometimes cause a flash of black when
    // resizing. See https://crbug.com/739724
    if (is_translucent_) {
      window_pos->flags |= SWP_NOCOPYBITS;
    }
  } else {
    // The window size isn't changing so there are no exposed pixels.
    exposed_pixels_ = gfx::Size();
  }

  if (ScopedFullscreenVisibility::IsHiddenForFullscreen(hwnd())) {
    // Prevent the window from being made visible if we've been asked to do so.
    // See comment in header as to why we might want this.
    window_pos->flags &= ~SWP_SHOWWINDOW;
  }

  if (window_pos->flags & SWP_HIDEWINDOW) {
    SetDwmFrameExtension(DwmFrameState::kOff);
  }

  SetMsgHandled(FALSE);
}

bool DidClientAreaSizeChange(const WINDOWPOS* window_pos) {
  return !(window_pos->flags & SWP_NOSIZE) ||
         window_pos->flags & SWP_FRAMECHANGED;
}

bool DidWindowPositionChange(const WINDOWPOS* window_pos) {
  return !(window_pos->flags & SWP_NOMOVE);
}

void HWNDMessageHandler::OnWindowPosChanged(WINDOWPOS* window_pos) {
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  if (DidClientAreaSizeChange(window_pos)) {
    ClientAreaSizeChanged();
  }
  if (!ref) {
    return;
  }
  if (window_pos->flags & SWP_FRAMECHANGED) {
    SetDwmFrameExtension(DwmFrameState::kOn);
  }
  if (window_pos->flags & SWP_SHOWWINDOW) {
    delegate_->HandleVisibilityChanged(true);
    SetDwmFrameExtension(DwmFrameState::kOn);
  } else if (window_pos->flags & SWP_HIDEWINDOW) {
    delegate_->HandleVisibilityChanged(false);
  }
  if (DidWindowPositionChange(window_pos)) {
    delegate_->HandleMoved();
  }
  UpdateDwmFrame();
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnWindowSizingFinished(UINT message,
                                                   WPARAM w_param,
                                                   LPARAM l_param) {
  // Check if a newer WM_WINDOWPOSCHANGING or WM_WINDOWPOSCHANGED have been
  // received after this message was posted.
  if (current_window_size_message_ != w_param) {
    return 0;
  }
  delegate_->HandleWindowSizeUnchanged();
  sent_window_size_changing_ = false;

  // The window size didn't actually change, so nothing was exposed that needs
  // to be filled black.
  exposed_pixels_ = gfx::Size();

  return 0;
}

void HWNDMessageHandler::UpdateDwmFrame() {
  gfx::Insets insets;
  if (ui::win::IsAeroGlassEnabled() &&
      delegate_->GetDwmFrameInsetsInPixels(&insets)) {
    MARGINS margins = {insets.left(), insets.right(), insets.top(),
                       insets.bottom()};
    DwmExtendFrameIntoClientArea(hwnd(), &margins);
  }
}

void HWNDMessageHandler::SetBoundsInternal(const gfx::Rect& bounds_in_pixels,
                                           bool force_size_changed) {
  gfx::Size old_size = GetClientAreaBounds().size();
  SetWindowPos(hwnd(), nullptr, bounds_in_pixels.x(), bounds_in_pixels.y(),
               bounds_in_pixels.width(), bounds_in_pixels.height(),
               SWP_NOACTIVATE | SWP_NOZORDER);

  // If HWND size is not changed, we will not receive standard size change
  // notifications. If |force_size_changed| is |true|, we should pretend size is
  // changed.
  if (old_size == bounds_in_pixels.size() && force_size_changed &&
      !background_fullscreen_hack_) {
    delegate_->HandleClientSizeChanged(GetClientAreaBounds().size());
  }
}

void HWNDMessageHandler::CheckAndHandleBackgroundFullscreenOnMonitor(
    HWND window) {
  HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);

  FullscreenWindowMonitorMap::iterator iter =
      fullscreen_monitor_map_.Get().find(monitor);
  if (iter != fullscreen_monitor_map_.Get().end()) {
    DCHECK(iter->second);
    if (window != iter->second->hwnd()) {
      iter->second->OnBackgroundFullscreen();
    }
  }
}

void HWNDMessageHandler::OnBackgroundFullscreen() {
  // Reduce the bounds of the window by 1px to ensure that Windows does
  // not treat this like a fullscreen window.
  MONITORINFO monitor_info = {sizeof(monitor_info)};
  GetMonitorInfo(MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY),
                 &monitor_info);
  gfx::Rect shrunk_rect(monitor_info.rcMonitor);
  shrunk_rect.set_height(shrunk_rect.height() - 1);
  background_fullscreen_hack_ = true;
  SetBoundsInternal(shrunk_rect, false);
  // Inform the taskbar that this window is no longer a fullscreen window so it
  // can bring itself to the top of the Z-Order. The taskbar heuristics to
  // detect fullscreen windows are not reliable. Marking it explicitly seems to
  // work around these problems.
  fullscreen_handler()->MarkFullscreen(false);
}

void HWNDMessageHandler::SizeWindowToAspectRatio(UINT param,
                                                 gfx::Rect* window_rect) {
  gfx::Size min_window_size;
  gfx::Size max_window_size;
  delegate_->GetMinMaxSize(&min_window_size, &max_window_size);
  min_window_size = delegate_->DIPToScreenSize(min_window_size);
  max_window_size = delegate_->DIPToScreenSize(max_window_size);
  // Add the native frame border size to the minimum and maximum size if the
  // view reports its size as the client size.
  if (HasSystemFrame()) {
    const gfx::Size non_client_size = GetNonClientSizeInPixels(hwnd());
    min_window_size.Enlarge(non_client_size.width(), non_client_size.height());
    if (max_window_size.width()) {
      max_window_size.Enlarge(non_client_size.width(), 0);
    }
    if (max_window_size.height()) {
      max_window_size.Enlarge(0, non_client_size.height());
    }
  }
  gfx::SizeRectToAspectRatio(GetWindowResizeEdge(param), aspect_ratio_.value(),
                             min_window_size, max_window_size, window_rect);
}

}  // namespace ui
