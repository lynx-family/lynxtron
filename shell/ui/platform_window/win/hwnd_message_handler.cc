// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/ui/platform_window/win/hwnd_message_handler.h"

#include <tchar.h>

#include <dwmapi.h>
#include <oleacc.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/win/windows_types.h"
#include "shell/api/dpi_win.h"
// #include "base/functional/callback_helpers.h"
#include "base/debug/gdi_debug_util_win.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
// #include "base/metrics/histogram_functions.h"
#include "base/strings/string_util_win.h"
// #include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/display/win/screen_win.h"
// #include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "shell/common/win_util.h"
// #include "shell/ui/base/win/internal_constants.h"
#include "shell/ui/base/win/shell.h"
// #include "shell/ui/events/event_constants.h"
// #include "shell/ui/events/event_utils.h"
// #include "shell/ui/events/keycodes/keyboard_code_conversion_win.h"
// #include "shell/ui/events/types/event_type.h"
// #include "shell/ui/events/win/system_event_state_lookup.h"
// #include "shell/ui/display/screen.h"
// #include "shell/ui/display/win/dpi.h"
// #include "shell/ui/display/win/screen_win.h"
#include "shell/ui/gfx/geometry/insets.h"
#include "shell/ui/gfx/geometry/resize_utils.h"
#include "shell/ui/gfx/win/hwnd_util.h"
#include "shell/ui/platform_window/win/fullscreen_handler.h"
#include "shell/ui/platform_window/win/hwnd_message_handler_delegate.h"
#include "shell/ui/platform_window/win/scoped_fullscreen_visibility.h"

namespace ui {

const wchar_t kIgnoreTouchMouseActivateForWindow[] =
    L"Chrome.IgnoreMouseActivate";

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

// Called from OnNCActivate.
// BOOL CALLBACK EnumChildWindowsForRedraw(HWND hwnd, LPARAM lparam) {
//  DWORD process_id;
//  GetWindowThreadProcessId(hwnd, &process_id);
//  int flags = RDW_INVALIDATE | RDW_NOCHILDREN | RDW_FRAME;
//  if (process_id == GetCurrentProcessId())
//    flags |= RDW_UPDATENOW;
//  RedrawWindow(hwnd, nullptr, nullptr, flags);
//  return TRUE;
//}

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

// Callback used to notify child windows that the top level window received a
// DWMCompositionChanged message.
BOOL CALLBACK SendDwmCompositionChanged(HWND window, LPARAM param) {
  SendMessage(window, WM_DWMCOMPOSITIONCHANGED, 0, 0);
  return TRUE;
}

// The thickness of an auto-hide taskbar in pixels.
// constexpr int kAutoHideTaskbarThicknessPx = 2;

bool IsTopLevelWindow(HWND window) {
  LONG style = ::GetWindowLong(window, GWL_STYLE);
  if (!(style & WS_CHILD)) {
    return true;
  }
  HWND parent = ::GetParent(window);
  return !parent || (parent == ::GetDesktopWindow());
}

// ui::EventType GetTouchEventType(POINTER_FLAGS pointer_flags) {
//   if (pointer_flags & POINTER_FLAG_DOWN)
//     return ui::ET_TOUCH_PRESSED;
//   if (pointer_flags & POINTER_FLAG_UPDATE)
//     return ui::ET_TOUCH_MOVED;
//   if (pointer_flags & POINTER_FLAG_UP)
//     return ui::ET_TOUCH_RELEASED;
//   return ui::ET_TOUCH_MOVED;
// }

// bool IsHitTestOnResizeHandle(LRESULT hittest) {
//   return hittest == HTRIGHT || hittest == HTLEFT || hittest == HTTOP ||
//          hittest == HTBOTTOM || hittest == HTTOPLEFT || hittest == HTTOPRIGHT
//          || hittest == HTBOTTOMLEFT || hittest == HTBOTTOMRIGHT;
// }

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

// int GetFlagsFromRawInputMessage(RAWINPUT* input) {
//   int flags = ui::EF_NONE;
//   if (input->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN)
//     flags |= ui::EF_LEFT_MOUSE_BUTTON;
//   if (input->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN)
//     flags |= ui::EF_RIGHT_MOUSE_BUTTON;
//   if (input->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN)
//     flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
//   if (input->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
//     flags |= ui::EF_BACK_MOUSE_BUTTON;
//   if (input->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
//     flags |= ui::EF_FORWARD_MOUSE_BUTTON;
//
//   return ui::GetModifiersFromKeyState() | flags;
// }

// constexpr auto kTouchDownContextResetTimeout =
//     base::TimeDelta::FromMilliseconds(500);

// Windows does not flag synthesized mouse messages from touch or pen in all
// cases. This causes us grief as we don't want to process touch and mouse
// messages concurrently. Hack as per msdn is to check if the time difference
// between the touch/pen message and the mouse move is within 500 ms and at the
// same location as the cursor.
// constexpr int kSynthesizedMouseMessagesTimeDifference = 500;

}  // namespace

// bool HWNDMessageHandlerDelegate::HasNativeFrame() const {
//   return false;

// static HWNDMessageHandler member initialization.
base::LazyInstance<HWNDMessageHandler::FullscreenWindowMonitorMap>::
    DestructorAtExit HWNDMessageHandler::fullscreen_monitor_map_ =
        LAZY_INSTANCE_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
// HWNDMessageHandler, public:

// Removed unused last_touch_or_pen_message_time_

HWNDMessageHandler::HWNDMessageHandler(HWNDMessageHandlerDelegate* delegate)
    : delegate_(delegate),
      fullscreen_handler_(new FullscreenHandler),
      waiting_for_close_now_(false),
      restored_enabled_(false),
      // current_cursor_(base::MakeRefCounted<ui::WinCursor>()),
      dpi_(0),
      called_enable_non_client_dpi_scaling_(false),
      lock_updates_count_(0),
      ignore_window_pos_changes_(false),
      last_monitor_(nullptr),
      is_first_nccalc_(true),
      menu_depth_(0),
      // id_generator_(0),
      // pen_processor_(&id_generator_, true),
      // touch_down_contexts_(0),
      dwm_transition_desired_(false),
      dwm_composition_enabled_(ui::win::IsDwmCompositionEnabled()),
      sent_window_size_changing_(false),
      did_return_uia_object_(false),
      background_fullscreen_hack_(false) {}
// pointer_events_for_touch_(::features::IsUsingWMPointerForTouch()) {}

HWNDMessageHandler::~HWNDMessageHandler() {
  // Prevent calls back into this class via WNDPROC now that we've been
  // destroyed.
  ClearUserData();
}

void HWNDMessageHandler::Init(HWND parent, const gfx::Rect& bounds) {
  TRACE_EVENT0("views", "HWNDMessageHandler::Init");
  GetMonitorAndRects(bounds.ToRECT(), &last_monitor_, &last_monitor_rect_,
                     &last_work_area_);

  initial_bounds_valid_ = !bounds.IsEmpty();

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

// void HWNDMessageHandler::InitModalType(ui::ModalType modal_type) {
//   if (modal_type == ui::MODAL_TYPE_NONE) {
//     return;
//   }
//   // We implement modality by crawling up the hierarchy of windows starting
//   // at the owner, disabling all of them so that they don't receive input
//   // messages.
//   HWND start = ::GetWindow(hwnd(), GW_OWNER);
//   while (start) {
//     ::EnableWindow(start, FALSE);
//     start = ::GetParent(start);
//   }
// }

void HWNDMessageHandler::Close() {
  if (!IsWindow(hwnd())) {
    return;  // No need to do anything.
  }

  // Let's hide ourselves right away.
  Hide();

  // Modal dialog windows disable their owner windows; re-enable them now so
  // they can activate as foreground windows upon this window's destruction.
  RestoreEnabledIfNecessary();

  // Re-enable flicks which removes the window property.
  // base::win::EnableFlicks(hwnd());

  if (!waiting_for_close_now_) {
    // And we delay the close so that if we are called from an ATL callback,
    // we don't destroy the window before the callback returned (as the caller
    // may delete ourselves on destroy and the ATL callback would still
    // dereference us when the callback returns).
    waiting_for_close_now_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&HWNDMessageHandler::CloseNow,
                                  msg_handler_weak_factory_.GetWeakPtr()));
  }
}

void HWNDMessageHandler::CloseNow() {
  // We may already have been destroyed if the selection resulted in a tab
  // switch which will have reactivated the browser window and closed us, so
  // we need to check to see if we're still a window before trying to destroy
  // ourself.
  waiting_for_close_now_ = false;
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

// void HWNDMessageHandler::SetRegion(HRGN region) {
//   custom_window_region_.reset(region);
//   ResetWindowRegion(true, true);
// }

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
  TRACE_EVENT0("views", "HWNDMessageHandler::Show");
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
    // NOTE: Be careful not to activate any windows here (for example, calling
    // ShowWindow(SW_HIDE) will automatically activate another window).  This
    // code can be called while a window is being deactivated, and activating
    // another window will screw up the activation that is already in progress.
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

// void HWNDMessageHandler::SetCapture() {
//   DCHECK(!HasCapture());
//   ::SetCapture(hwnd());
// }

// void HWNDMessageHandler::ReleaseCapture() {
//   if (HasCapture()) {
//     ::ReleaseCapture();
//   }
// }

// bool HWNDMessageHandler::HasCapture() const {
//   return ::GetCapture() == hwnd();
// }

// void HWNDMessageHandler::SetVisibilityChangedAnimationsEnabled(bool enabled)
// {
//   int dwm_value = enabled ? FALSE : TRUE;
//   DwmSetWindowAttribute(hwnd(), DWMWA_TRANSITIONS_FORCEDISABLED, &dwm_value,
//                         sizeof(dwm_value));
// }

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

// void HWNDMessageHandler::SetCursor(scoped_refptr<ui::WinCursor> cursor) {
//   DCHECK(cursor);
//
//   TRACE_EVENT1("ui,input", "HWNDMessageHandler::SetCursor", "cursor",
//                static_cast<const void*>(cursor->hcursor()));
//   ::SetCursor(cursor->hcursor());
//   current_cursor_ = cursor;
// }

void HWNDMessageHandler::FrameTypeChanged() {
  needs_dwm_frame_clear_ = true;
  if (IsFrameSystemDrawn()) {
    dwm_transition_desired_ = true;
  }
  if (!dwm_transition_desired_ || !IsFullscreen()) {
    PerformDwmTransition();
  }
}

// void HWNDMessageHandler::SetWindowIcons(const gfx::ImageSkia& window_icon,
//                                         const gfx::ImageSkia& app_icon) {
//   if (!window_icon.isNull()) {
//     base::win::ScopedHICON previous_icon = std::move(window_icon_);
//     window_icon_ = IconUtil::CreateHICONFromSkBitmap(*window_icon.bitmap());
//     SendMessage(hwnd(), WM_SETICON, ICON_SMALL,
//                 reinterpret_cast<LPARAM>(window_icon_.get()));
//   }
//   if (!app_icon.isNull()) {
//     base::win::ScopedHICON previous_icon = std::move(app_icon_);
//     app_icon_ = IconUtil::CreateHICONFromSkBitmap(*app_icon.bitmap());
//     SendMessage(hwnd(), WM_SETICON, ICON_BIG,
//                 reinterpret_cast<LPARAM>(app_icon_.get()));
//   }
// }

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
  // If we are out of fullscreen and there was a pending DWM transition for the
  // window, then go ahead and do it now.
  if (!fullscreen && dwm_transition_desired_) {
    PerformDwmTransition();
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

  // Windows cannot have WS_THICKFRAME set if translucent.
  // See CalculateWindowStylesFromInitParams().
  if (delegate_->CanResize() && !is_translucent_) {
    style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    if (!delegate_->CanMaximize()) {
      style &= ~WS_MAXIMIZEBOX;
    }
  } else {
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }
  if (delegate_->CanMinimize()) {
    style |= WS_MINIMIZEBOX;
  } else {
    style &= ~WS_MINIMIZEBOX;
  }
  SetWindowLong(hwnd(), GWL_STYLE, style);
}

// bool HWNDMessageHandler::HasChildRenderingWindow() {
//  return false;
//   // This can change dynamically if the system switches between GPU and
//   // software rendering.
//   //return gfx::RenderingWindowManager::GetInstance()->HasValidChildWindow(
//   //    hwnd());
// }

// std::unique_ptr<aura::ScopedEnableUnadjustedMouseEvents>
// HWNDMessageHandler::RegisterUnadjustedMouseEvent() {
//   std::unique_ptr<ScopedEnableUnadjustedMouseEventsWin> scoped_enable =
//       ScopedEnableUnadjustedMouseEventsWin::StartMonitor(this);
//
//   DCHECK(using_wm_input_);
//   return scoped_enable;
// }

// bool HWNDMessageHandler::HasNativeFrame() {
//   return delegate_->HasNativeFrame();
// }

////////////////////////////////////////////////////////////////////////////////
// HWNDMessageHandler, gfx::WindowImpl overrides:

// HICON HWNDMessageHandler::GetDefaultWindowIcon() const {
//   return use_system_default_icon_
//              ? nullptr
//              : ViewsDelegate::GetInstance()->GetDefaultWindowIcon();
// }
//
// HICON HWNDMessageHandler::GetSmallWindowIcon() const {
//   return use_system_default_icon_
//              ? nullptr
//              : ViewsDelegate::GetInstance()->GetSmallWindowIcon();
// }

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

// Removed unused TrackMouseEvents

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
    LOG(INFO) << " GetClientAreaInsets";
    return true;
  }
  DCHECK(insets->IsEmpty());

  // Returning false causes the default handling in OnNCCalcSize() to
  // be invoked.
  // if (!delegate_->HasNonClientView() || HasSystemFrame()) {
  //   return false;
  // }

  if (HasSystemFrame()) {
    LOG(INFO) << " HasSystemFrame";
    return false;
  }

  if (IsMaximized()) {
    // Windows automatically adds a standard width border to all sides when a
    // window is maximized.
    // TODO(Guo Xi): add ui::GetFrameThickness(monitor)
    int frame_thickness = 1;  // ui::GetFrameThickness(monitor);
    if (!delegate_->HasFrame()) {
      frame_thickness -= 1;
    }
    *insets = gfx::Insets(frame_thickness);
    return true;
  }

  *insets = gfx::Insets();
  return true;
}

// void HWNDMessageHandler::ResetWindowRegion(bool force, bool redraw) {
//   // A native frame uses the native window region, and we don't want to mess
//   // with it.
//   // WS_EX_LAYERED automatically makes clicks on transparent pixels fall
//   // through, but that isn't the case when using Direct3D to draw transparent
//   // windows. So we route translucent windows throught to the delegate to
//   // allow for a custom hit mask.
//   if (!is_translucent_ && !custom_window_region_.is_valid() &&
//       (IsFrameSystemDrawn())) {
//     // (IsFrameSystemDrawn() || !delegate_->HasNonClientView())) {
//     if (force) {
//       SetWindowRgn(hwnd(), nullptr, redraw);
//     }
//     return;
//   }

//   // Changing the window region is going to force a paint. Only change the
//   // window region if the region really differs.
//   base::win::ScopedGDIObject<HRGN> current_rgn(CreateRectRgn(0, 0, 0, 0));
//   GetWindowRgn(hwnd(), current_rgn.get());

//   RECT window_rect;
//   GetWindowRect(hwnd(), &window_rect);
//   base::win::ScopedGDIObject<HRGN> new_region;
//   if (custom_window_region_.is_valid()) {
//     new_region.reset(CreateRectRgn(0, 0, 0, 0));
//     CombineRgn(new_region.get(), custom_window_region_.get(), nullptr,
//                RGN_COPY);
//   } else if (IsMaximized()) {
//     HMONITOR monitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONEAREST);
//     MONITORINFO mi;
//     mi.cbSize = sizeof mi;
//     GetMonitorInfo(monitor, &mi);
//     RECT work_rect = mi.rcWork;
//     OffsetRect(&work_rect, -window_rect.left, -window_rect.top);
//     new_region.reset(CreateRectRgnIndirect(&work_rect));
//   } else {
//     /*SkPath window_mask;
//     delegate_->GetWindowMask(gfx::Size(window_rect.right - window_rect.left,
//                                        window_rect.bottom - window_rect.top),
//                              &window_mask);
//     if (!window_mask.isEmpty())
//       new_region.reset(gfx::CreateHRGNFromSkPath(window_mask));*/
//   }

//   const bool has_current_region = current_rgn != nullptr;
//   const bool has_new_region = new_region != nullptr;
//   if (has_current_region != has_new_region ||
//       (has_current_region && !EqualRgn(current_rgn.get(), new_region.get())))
//       {
//     // SetWindowRgn takes ownership of the HRGN.
//     SetWindowRgn(hwnd(), new_region.release(), redraw);
//   }
// }

void HWNDMessageHandler::UpdateDwmNcRenderingPolicy() {
  if (IsFullscreen()) {
    return;
  }

  // TODO(Guo Xi): GetFrameMode() is not implemented yet.
  DWMNCRENDERINGPOLICY policy = DWMNCRP_DISABLED;
  // custom_window_region_.is_valid() ||
  //         delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN
  //     ? DWMNCRP_DISABLED
  //     : DWMNCRP_ENABLED;

  DwmSetWindowAttribute(hwnd(), DWMWA_NCRENDERING_POLICY, &policy,
                        sizeof(DWMNCRENDERINGPOLICY));
}

LRESULT HWNDMessageHandler::DefWindowProcWithRedrawLock(UINT message,
                                                        WPARAM w_param,
                                                        LPARAM l_param) {
  LRESULT result = DefWindowProc(hwnd(), message, w_param, l_param);
  return result;
}

void HWNDMessageHandler::LockUpdates() {
  if (++lock_updates_count_ == 1) {
    SetWindowLong(hwnd(), GWL_STYLE,
                  GetWindowLong(hwnd(), GWL_STYLE) & ~WS_VISIBLE);
  }
}

void HWNDMessageHandler::UnlockUpdates() {
  if (--lock_updates_count_ <= 0) {
    SetWindowLong(hwnd(), GWL_STYLE,
                  GetWindowLong(hwnd(), GWL_STYLE) | WS_VISIBLE);
    lock_updates_count_ = 0;
  }
}

void HWNDMessageHandler::ForceRedrawWindow(int attempts) {
  // if (ui::IsWorkstationLocked()) {
  //   // Presents will continue to fail as long as the input desktop is
  //   // unavailable.
  //   if (--attempts <= 0)
  //     return;
  //   base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
  //       FROM_HERE,
  //       base::BindOnce(&HWNDMessageHandler::ForceRedrawWindow,
  //                      msg_handler_weak_factory_.GetWeakPtr(), attempts),
  //       base::TimeDelta::FromMilliseconds(500));
  //   return;
  // }
  InvalidateRect(hwnd(), nullptr, FALSE);
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

void HWNDMessageHandler::OnCaptureChanged(HWND window) {
  delegate_->HandleCaptureLost();
}

void HWNDMessageHandler::OnClose() {
  delegate_->HandleClose();
}

void HWNDMessageHandler::OnCommand(UINT notification_code,
                                   int command,
                                   HWND window) {
  // If the notification code is > 1 it means it is control specific and we
  // should ignore it.
  if (notification_code > 1 || delegate_->HandleAppCommand(command)) {
    SetMsgHandled(FALSE);
  }
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
    SetWindowLong(hwnd(), GWL_STYLE,
                  GetWindowLong(hwnd(), GWL_STYLE) & ~WS_CAPTION);
    SendFrameChanged();
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

  // If the window was initialized with a specific size/location then we know
  // the DPI and thus must initialize dpi_ now. See https://crbug.com/1282804
  // for details.
  if (initial_bounds_valid_) {
    dpi_ = lynxtron::GetDPIForHWND(hwnd());
  }

  // TODO(beng): move more of NWW::OnCreate here.
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
  TRACE_EVENT0("ui", "HWNDMessageHandler::OnDisplayChange");

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
  TRACE_EVENT0("ui", "HWNDMessageHandler::OnDwmCompositionChanged");

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

  // TRACE_EVENT("ui", "HWNDMessageHandler::OnDpiChanged",
  //             [&](perfetto::EventContext ctx) {
  //               perfetto::protos::pbzero::ChromeWindowHandleEventInfo* args =
  //                   ctx.event()->set_chrome_window_handle_event_info();
  //               args->set_dpi(LOWORD(w_param));
  //             });

  int dpi;
  float scaling_factor;
  // if (display::Display::HasForceDeviceScaleFactor()) {
  //   scaling_factor = display::Display::GetForcedDeviceScaleFactor();
  //   dpi = display::win::GetDPIFromScalingFactor(scaling_factor);
  // } else {
  dpi = LOWORD(w_param);
  scaling_factor = lynxtron::GetScalingFactorFromDPI(dpi);
  // }

  // The first WM_DPICHANGED originates from EnableChildWindowDpiMessage during
  // initialization. We don't want to propagate this as the client is already
  // set at the current scale factor and may cause the window to display too
  // soon. See http://crbug.com/625076.
  if (dpi_ == 0) {
    // See https://crbug.com/1252564 for why we need to ignore the first
    // OnDpiChanged message in this way.
    dpi_ = dpi;
    return 0;
  }

  dpi_ = dpi;
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
  if (true) {
    RECT client_rect, window_rect;
    GetClientRect(hwnd(), &client_rect);
    GetWindowRect(hwnd(), &window_rect);
    CR_DEFLATE_RECT(&window_rect, &client_rect);
    min_window_size.Enlarge(window_rect.right - window_rect.left,
                            window_rect.bottom - window_rect.top);
    // Either axis may be zero, so enlarge them independently.
    if (max_window_size.width()) {
      max_window_size.Enlarge(window_rect.right - window_rect.left, 0);
    }
    if (max_window_size.height()) {
      max_window_size.Enlarge(0, window_rect.bottom - window_rect.top);
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

// LRESULT HWNDMessageHandler::OnMouseActivate(UINT message,
//                                             WPARAM w_param,
//                                             LPARAM l_param) {
//   // Please refer to the comments in the header for the touch_down_contexts_
//   // member for the if statement below.
//   if (touch_down_contexts_) {
//     return MA_NOACTIVATE;
//   }

//   // On Windows, if we select the menu item by touch and if the window at the
//   // location is another window on the same thread, that window gets a
//   // WM_MOUSEACTIVATE message and ends up activating itself, which is not
//   // correct. We workaround this by setting a property on the window at the
//   // current cursor location. We check for this property in our
//   // WM_MOUSEACTIVATE handler and don't activate the window if the property
//   is
//   // set.
//   if (::GetProp(hwnd(), ui::kIgnoreTouchMouseActivateForWindow)) {
//     ::RemoveProp(hwnd(), ui::kIgnoreTouchMouseActivateForWindow);
//     return MA_NOACTIVATE;
//   }

//   // TODO(beng): resolve this with the GetWindowLong() check on the
//   subsequent
//   //             line.
//   // if (delegate_->HasNonClientView()) {
//   if (delegate_->CanActivate()) {
//     return MA_ACTIVATE;
//   }
//   if (delegate_->WantsMouseEventsWhenInactive()) {
//     return MA_NOACTIVATE;
//   }
//   return MA_NOACTIVATEANDEAT;
//   // }
//   if (GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_NOACTIVATE) {
//     return MA_NOACTIVATE;
//   }
//   SetMsgHandled(FALSE);
//   return MA_ACTIVATE;
// }

// LRESULT HWNDMessageHandler::OnMouseRange(UINT message,
//                                          WPARAM w_param,
//                                          LPARAM l_param) {
//   return HandleMouseEventInternal(message, w_param, l_param, true);
// }

// On some systems with a high-resolution track pad and running Windows 10,
// using the scrolling gesture (two-finger scroll) on the track pad
// causes it to also generate a WM_POINTERDOWN message if the window
// isn't focused. This leads to a WM_POINTERACTIVATE message and the window
// gaining focus and coming to the front. This code detects a
// WM_POINTERACTIVATE coming from the track pad and kills the activation
// of the window. NOTE: most other trackpad messages come in as mouse
// messages, including WM_MOUSEWHEEL instead of WM_POINTERWHEEL.
// LRESULT HWNDMessageHandler::OnPointerActivate(UINT message,
//                                               WPARAM w_param,
//                                               LPARAM l_param) {
//   using GetPointerTypeFn = BOOL(WINAPI*)(UINT32, POINTER_INPUT_TYPE*);
//   UINT32 pointer_id = GET_POINTERID_WPARAM(w_param);
//   POINTER_INPUT_TYPE pointer_type;
//   static const auto get_pointer_type = reinterpret_cast<GetPointerTypeFn>(
//       base::win::GetUser32FunctionPointer("GetPointerType"));
//   if (get_pointer_type && get_pointer_type(pointer_id, &pointer_type) &&
//       pointer_type == PT_TOUCHPAD) {
//     return PA_NOACTIVATE;
//   }
//   SetMsgHandled(FALSE);
//   return -1;
// }

// LRESULT HWNDMessageHandler::OnPointerEvent(UINT message,
//                                            WPARAM w_param,
//                                            LPARAM l_param) {
//   // WM_POINTER is not supported on Windows 7.
//   if (base::win::GetVersion() == base::win::Version::WIN7) {
//     SetMsgHandled(FALSE);
//     return -1;
//   }

//   UINT32 pointer_id = GET_POINTERID_WPARAM(w_param);
//   using GetPointerTypeFn = BOOL(WINAPI*)(UINT32, POINTER_INPUT_TYPE*);
//   POINTER_INPUT_TYPE pointer_type;
//   static const auto get_pointer_type = reinterpret_cast<GetPointerTypeFn>(
//       base::win::GetUser32FunctionPointer("GetPointerType"));
//   // If the WM_POINTER messages are not sent from a stylus device, then we do
//   // not handle them to make sure we do not change the current behavior of
//   // touch and mouse inputs.
//   if (!get_pointer_type || !get_pointer_type(pointer_id, &pointer_type)) {
//     SetMsgHandled(FALSE);
//     return -1;
//   }

//   // |HandlePointerEventTypePenClient| assumes all pen events happen on the
//   // client area, so WM_NCPOINTER messages sent to it would eventually be
//   // dropped and the native frame wouldn't be able to respond to pens.
//   // |HandlePointerEventTypeTouchOrNonClient| handles non-client area
//   messages
//   // properly. Since we don't need to distinguish between pens and fingers in
//   // non-client area, route the messages to that method.
//   if (pointer_type == PT_PEN &&
//       (message == WM_NCPOINTERDOWN || message == WM_NCPOINTERUP ||
//        message == WM_NCPOINTERUPDATE)) {
//     pointer_type = PT_TOUCH;
//   }

//   // TODO(Guo Xi)
//   // switch (pointer_type) {
//   //   case PT_PEN:
//   //     return HandlePointerEventTypePenClient(message, w_param, l_param);
//   //   case PT_TOUCH:
//   //     if (pointer_events_for_touch_) {
//   //       return HandlePointerEventTypeTouchOrNonClient(message, w_param,
//   //                                                     l_param);
//   //     }
//   //     FALLTHROUGH;
//   //   default:
//   //     break;
//   // }
//   SetMsgHandled(FALSE);
//   return -1;
// }

LRESULT HWNDMessageHandler::OnInputEvent(UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  HRAWINPUT input_handle = reinterpret_cast<HRAWINPUT>(l_param);

  // Get the size of the input record.
  UINT size = 0;
  UINT result = ::GetRawInputData(input_handle, RID_INPUT, nullptr, &size,
                                  sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }
  DCHECK_EQ(0u, result);

  // Retrieve the input record.
  auto buffer = std::make_unique<uint8_t[]>(size);
  RAWINPUT* input = reinterpret_cast<RAWINPUT*>(buffer.get());
  result = ::GetRawInputData(input_handle, RID_INPUT, buffer.get(), &size,
                             sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }
  DCHECK_EQ(size, result);
  return ::DefRawInputProc(&input, 1, sizeof(RAWINPUTHEADER));
}

void HWNDMessageHandler::OnMove(const gfx::Point& point) {
  delegate_->HandleMove();
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnMoving(UINT param, RECT* new_bounds) {
  delegate_->HandleMoving(new_bounds);
}

LRESULT HWNDMessageHandler::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // We only override the default handling if we need to specify a custom
  // non-client edge width. Note that in most cases "no insets" means no
  // custom width, but in fullscreen mode or when the NonClientFrameView
  // requests it, we want a custom width of 0.

  // Let User32 handle the first nccalcsize for captioned windows
  // so it updates its internal structures (specifically caption-present)
  // Without this Tile & Cascade windows won't work.
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
  LOG(INFO) << "insets: " << (got_insets ? "true" : "false");
  LOG(INFO) << " fullscreen " << IsFullscreen();
  LOG(INFO) << " mode " << mode;
  LOG(INFO) << " hasframe " << delegate_->HasFrame();
  if (!got_insets && !IsFullscreen() && !(mode && !delegate_->HasFrame())) {
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
    // TODO(Guo Xi): support app bar auto hide edges.
    // const int autohide_edges = GetAppbarAutohideEdges(monitor);
    // if (autohide_edges & ViewsDelegate::EDGE_LEFT) {
    //   client_rect->left += kAutoHideTaskbarThicknessPx;
    // }
    // if (autohide_edges & ViewsDelegate::EDGE_TOP) {
    //   if (IsFrameSystemDrawn()) {
    //     // Tricky bit.  Due to a bug in DwmDefWindowProc()'s handling of
    //     // WM_NCHITTEST, having any nonclient area atop the window causes the
    //     // caption buttons to draw onscreen but not respond to mouse
    //     // hover/clicks.
    //     // So for a taskbar at the screen top, we can't push the
    //     // client_rect->top down; instead, we move the bottom up by one
    //     pixel,
    //     // which is the smallest change we can make and still get a client
    //     area
    //     // less than the screen size. This is visibly ugly, but there seems
    //     to
    //     // be no better solution.
    //     --client_rect->bottom;
    //   } else {
    //     client_rect->top += kAutoHideTaskbarThicknessPx;
    //   }
    // }
    // if (autohide_edges & ViewsDelegate::EDGE_RIGHT) {
    //   client_rect->right -= kAutoHideTaskbarThicknessPx;
    // }
    // if (autohide_edges & ViewsDelegate::EDGE_BOTTOM) {
    //   client_rect->bottom -= kAutoHideTaskbarThicknessPx;
    // }

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
  // if (!delegate_->HasNonClientView()) {
  //   SetMsgHandled(FALSE);
  //   return 0;
  // }

  // Some views may overlap the non client area of the window.
  // This means that we should look for these views before handing the
  // hittest message off to DWM or DefWindowProc.
  // If the hittest returned from the search for a view returns HTCLIENT
  // then it means that we have a view overlapping the non client area.
  // In all other cases we can fallback to the system default handling.

  // Allow the NonClientView to handle the hittest to see if we have a view
  // overlapping the non client area of the window.
  // POINT temp = {point.x(), point.y()};
  // MapWindowPoints(HWND_DESKTOP, hwnd(), &temp, 1);
  // int component = delegate_->GetNonClientComponent(gfx::Point(temp));
  // if (component == HTCLIENT) {
  //   return component;
  // }

  // If the DWM is rendering the window controls, we need to give the DWM's
  // default window procedure first chance to handle hit testing.
  if (HasSystemFrame()) {
    LRESULT result;
    if (DwmDefWindowProc(hwnd(), WM_NCHITTEST, 0,
                         MAKELPARAM(point.x(), point.y()), &result)) {
      return result;
    }
  }

  // If the point is specified as custom or system nonclient item, return it.
  // if (component != HTNOWHERE) {
  //   return component;
  // }

  // Otherwise, we let Windows do all the native frame non-client handling for
  // us.
  LRESULT hit_test_code =
      DefWindowProc(hwnd(), WM_NCHITTEST, 0, MAKELPARAM(point.x(), point.y()));
  return hit_test_code;
}

// void HWNDMessageHandler::OnNCPaint(HRGN rgn) {
//   if (IsFrameSystemDrawn()) {
//     SetMsgHandled(FALSE);
//     return;
//   }
//   RECT window_rect;
//   GetWindowRect(hwnd(), &window_rect);
//   RECT dirty_region;
//   // A value of 1 indicates paint all.
//   if (!rgn || rgn == reinterpret_cast<HRGN>(1)) {
//     dirty_region.left = 0;
//     dirty_region.top = 0;
//     dirty_region.right = window_rect.right - window_rect.left;
//     dirty_region.bottom = window_rect.bottom - window_rect.top;
//   } else {
//     RECT rgn_bounding_box;
//     GetRgnBox(rgn, &rgn_bounding_box);
//     if (!IntersectRect(&dirty_region, &rgn_bounding_box, &window_rect)) {
//       SetMsgHandled(FALSE);
//       return;  // Dirty region doesn't intersect window bounds, bail.
//     }

//     // rgn_bounding_box is in screen coordinates. Map it to window
//     coordinates. OffsetRect(&dirty_region, -window_rect.left,
//     -window_rect.top);
//   }

//   // We only do non-client painting if we're not using the system frame.
//   // It's required to avoid some native painting artifacts from appearing
//   when
//   // the window is resized.
//   // if (!delegate_->HasNonClientView() || IsFrameSystemDrawn()) {
//   if (IsFrameSystemDrawn()) {
//     if (ui::win::IsAeroGlassEnabled()) {
//       // The default WM_NCPAINT handler under Aero Glass doesn't clear the
//       // nonclient area, so it'll remain the default white color. That area
//       is
//       // invisible initially (covered by the window border) but can become
//       // temporarily visible on maximizing or fullscreening, so clear it
//       here. HDC dc = GetWindowDC(hwnd()); RECT client_rect;
//       ::GetClientRect(hwnd(), &client_rect);
//       ::MapWindowPoints(hwnd(), nullptr,
//       reinterpret_cast<POINT*>(&client_rect),
//                         2);
//       ::OffsetRect(&client_rect, -window_rect.left, -window_rect.top);
//       // client_rect now is in window space.

//       base::win::ScopedGDIObject<HRGN> base(
//           ::CreateRectRgnIndirect(&dirty_region));
//       base::win::ScopedGDIObject<HRGN> client(
//           ::CreateRectRgnIndirect(&client_rect));
//       base::win::ScopedGDIObject<HRGN> nonclient(::CreateRectRgn(0, 0, 0,
//       0));
//       ::CombineRgn(nonclient.get(), base.get(), client.get(), RGN_DIFF);

//       ::SelectClipRgn(dc, nonclient.get());
//       HBRUSH brush = CreateSolidBrush(0);
//       ::FillRect(dc, &dirty_region, brush);
//       ::DeleteObject(brush);
//       ::ReleaseDC(hwnd(), dc);
//     }
//     SetMsgHandled(FALSE);
//     return;
//   }

//   gfx::Size root_view_size = delegate_->GetRootViewSize();
//   if (gfx::Size(window_rect.right - window_rect.left,
//                 window_rect.bottom - window_rect.top) != root_view_size) {
//     // If the size of the window differs from the size of the root view it
//     // means we're being asked to paint before we've gotten a WM_SIZE. This
//     can
//     // happen when the user is interactively resizing the window. To avoid
//     // mass flickering we don't do anything here. Once we get the WM_SIZE
//     we'll
//     // reset the region of the window which triggers another WM_NCPAINT and
//     // all is well.
//     return;
//   }
//   // delegate_->HandlePaintAccelerated(gfx::Rect(dirty_region));

//   // When using a custom frame, we want to avoid calling DefWindowProc()
//   since
//   // that may render artifacts.
//   SetMsgHandled(delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN);
// }

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

// LRESULT HWNDMessageHandler::OnNotify(int w_param, NMHDR* l_param) {
//   LRESULT l_result = 0;
//   SetMsgHandled(delegate_->HandleTooltipNotify(w_param, l_param, &l_result));
//   return l_result;
// }

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
    HBRUSH brush = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

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

    // delegate_->HandlePaintAccelerated(gfx::Rect(ps.rcPaint));
  }

  exposed_pixels_ = gfx::Size();

  EndPaint(hwnd(), &ps);
}

// LRESULT HWNDMessageHandler::OnReflectedMessage(UINT message,
//                                                WPARAM w_param,
//                                                LPARAM l_param) {
//   SetMsgHandled(FALSE);
//   return 0;
// }

// LRESULT HWNDMessageHandler::OnScrollMessage(UINT message,
//                                             WPARAM w_param,
//                                             LPARAM l_param) {
//   // MSG msg = {hwnd(), message, w_param, l_param,
//   //            static_cast<DWORD>(GetMessageTime())};
//   // ui::ScrollEvent event(msg);
//   // delegate_->HandleScrollEvent(&event);
//   return 0;
// }

// LRESULT HWNDMessageHandler::OnSetCursor(UINT message,
//                                         WPARAM w_param,
//                                         LPARAM l_param) {
//   // current_cursor_ must be a ui::WinCursor, so that custom image cursors
//   are
//   // properly ref-counted. cursor below is only used for system cursors and
//   // doesn't replace the current cursor so an HCURSOR can be used directly.
//   wchar_t* cursor = IDC_ARROW;
//   // Reimplement the necessary default behavior here. Calling DefWindowProc
//   can
//   // trigger weird non-client painting for non-glass windows with custom
//   frames.
//   // Using a ScopedRedrawLock to prevent caption rendering artifacts may
//   allow
//   // content behind this window to incorrectly paint in front of this window.
//   // Invalidating the window to paint over either set of artifacts is not
//   ideal. switch (LOWORD(l_param)) {
//     case HTSIZE:
//       cursor = IDC_SIZENWSE;
//       break;
//     case HTLEFT:
//     case HTRIGHT:
//       cursor = IDC_SIZEWE;
//       break;
//     case HTTOP:
//     case HTBOTTOM:
//       cursor = IDC_SIZENS;
//       break;
//     case HTTOPLEFT:
//     case HTBOTTOMRIGHT:
//       cursor = IDC_SIZENWSE;
//       break;
//     case HTTOPRIGHT:
//     case HTBOTTOMLEFT:
//       cursor = IDC_SIZENESW;
//       break;
//     case HTCLIENT:
//       // SetCursor(current_cursor_);
//       return 1;
//     case LOWORD(HTERROR):  // Use HTERROR's LOWORD value for valid
//     comparison.
//       SetMsgHandled(FALSE);
//       break;
//     default:
//       // Use the default value, IDC_ARROW.
//       break;
//   }
//   ::SetCursor(LoadCursor(nullptr, cursor));
//   return 1;
// }

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

void HWNDMessageHandler::OnSize(UINT param, const gfx::Size& size) {
  if (DidMinimizedChange(last_size_param_, param) && IsTopLevelWindow(hwnd())) {
    delegate_->HandleWindowMinimizedOrRestored(param != SIZE_MINIMIZED);
  }
  last_size_param_ = param;

  RedrawWindow(hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
  // ResetWindowRegion is going to trigger WM_NCPAINT. By doing it after we've
  // invoked OnSize we ensure the RootView has been laid out.
  // ResetWindowRegion(false, true);
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
    if (window_bounds_change || window_control_action) {
      // DestroyAXSystemCaret();
    }
    if (window_bounds_change && !IsVisible()) {
      // Circumvent ScopedRedrawLocks and force visibility before entering a
      // resize or move modal loop to get continuous sizing/moving feedback.
      SetWindowLong(hwnd(), GWL_STYLE,
                    GetWindowLong(hwnd(), GWL_STYLE) | WS_VISIBLE);
    }
  }

  // Handle SC_KEYMENU, which means that the user has pressed the ALT
  // key and released it, so we should focus the menu bar.
  // if ((notification_code & sc_mask) == SC_KEYMENU && point.x() == 0) {
  //  int modifiers = ui::EF_NONE;
  //  if (ui::win::IsShiftPressed())
  //    modifiers |= ui::EF_SHIFT_DOWN;
  //  if (ui::win::IsCtrlPressed())
  //    modifiers |= ui::EF_CONTROL_DOWN;
  //  // Retrieve the status of shift and control keys to prevent consuming
  //  // shift+alt keys, which are used by Windows to change input languages.
  //  // ui::Accelerator accelerator(ui::KeyboardCodeForWindowsKeyCode(VK_MENU),
  //  //                            modifiers);
  //  // delegate_->HandleAccelerator(accelerator);
  //  return;
  //}

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
// ui::NativeThemeWin::CloseHandles();

void HWNDMessageHandler::OnTimeChange() {
  // Call NowFromSystemTime() to force base::Time to re-initialize the clock
  // from system time. Otherwise base::Time::Now() might continue to reflect the
  // old system clock for some amount of time. See https://crbug.com/672906#c5
  base::Time::NowFromSystemTime();
}

void HWNDMessageHandler::OnWindowPosChanging(WINDOWPOS* window_pos) {
  TRACE_EVENT0("ui", "HWNDMessageHandler::OnWindowPosChanging");

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

void HWNDMessageHandler::OnWindowPosChanged(WINDOWPOS* window_pos) {
  TRACE_EVENT0("ui", "HWNDMessageHandler::OnWindowPosChanged");

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

void HWNDMessageHandler::OnSessionChange(WPARAM status_code,
                                         const bool* is_current_session) {
  // Direct3D presents are ignored while the screen is locked, so force the
  // window to be redrawn on unlock.
  if (status_code == WTS_SESSION_UNLOCK) {
    ForceRedrawWindow(10);
  }
}

// void HWNDMessageHandler::ResetTouchDownContext() {
//   touch_down_contexts_--;
// }

// Removed unused IsSynthesizedMouseMessage

void HWNDMessageHandler::PerformDwmTransition() {
  dwm_transition_desired_ = false;

  UpdateDwmNcRenderingPolicy();
  // Don't redraw the window here, because we need to hide and show the window
  // which will also trigger a redraw.
  // ResetWindowRegion(true, false);
  // The non-client view needs to update too.
  delegate_->HandleFrameChanged();
  // This calls DwmExtendFrameIntoClientArea which must be called when DWM
  // composition state changes.
  UpdateDwmFrame();

  if (IsVisible() && IsFrameSystemDrawn()) {
    // For some reason, we need to hide the window after we change from a custom
    // frame to a native frame.  If we don't, the client area will be filled
    // with black.  This seems to be related to an interaction between DWM and
    // SetWindowRgn, but the details aren't clear. Additionally, we need to
    // specify SWP_NOZORDER here, otherwise if you have multiple chrome windows
    // open they will re-appear with a non-deterministic Z-order.
    // Note: caused http://crbug.com/895855, where a laptop lid close+reopen
    // puts window in the background but acts like a foreground window. Fixed by
    // not calling this unless DWM composition actually changes. Finally, since
    // we don't want windows stealing focus if they're not already active, we
    // set SWP_NOACTIVATE.
    UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;
    SetWindowPos(hwnd(), nullptr, 0, 0, 0, 0, flags | SWP_HIDEWINDOW);
    SetWindowPos(hwnd(), nullptr, 0, 0, 0, 0, flags | SWP_SHOWWINDOW);
  }
  // WM_DWMCOMPOSITIONCHANGED is only sent to top level windows, however we want
  // to notify our children too, since we can have MDI child windows who need to
  // update their appearance.
  EnumChildWindows(hwnd(), &SendDwmCompositionChanged, NULL);
}

void HWNDMessageHandler::UpdateDwmFrame() {
  TRACE_EVENT0("ui", "HWNDMessageHandler::UpdateDwmFrame");

  gfx::Insets insets;
  if (ui::win::IsAeroGlassEnabled() &&
      delegate_->GetDwmFrameInsetsInPixels(&insets)) {
    MARGINS margins = {insets.left(), insets.right(), insets.top(),
                       insets.bottom()};
    DwmExtendFrameIntoClientArea(hwnd(), &margins);
  }
}

// Removed unused HandleMouseInputForCaption

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
    // ResetWindowRegion(false, true);
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

// void HWNDMessageHandler::DestroyAXSystemCaret() {
//   // ax_system_caret_ = nullptr;
// }

void HWNDMessageHandler::SizeWindowToAspectRatio(UINT param,
                                                 gfx::Rect* window_rect) {
  gfx::Size min_window_size;
  gfx::Size max_window_size;
  delegate_->GetMinMaxSize(&min_window_size, &max_window_size);
  min_window_size = delegate_->DIPToScreenSize(min_window_size);
  max_window_size = delegate_->DIPToScreenSize(max_window_size);
  // Add the native frame border size to the minimum and maximum size if the
  // view reports its size as the client size.
  if (true) {
    RECT client_rect, rect;
    GetClientRect(hwnd(), &client_rect);
    GetWindowRect(hwnd(), &rect);
    CR_DEFLATE_RECT(&rect, &client_rect);
    min_window_size.Enlarge(rect.right - rect.left, rect.bottom - rect.top);
    // Either axis may be zero, so enlarge them independently.
    if (max_window_size.width()) {
      max_window_size.Enlarge(rect.right - rect.left, 0);
    }
    if (max_window_size.height()) {
      max_window_size.Enlarge(0, rect.bottom - rect.top);
    }
  }
  gfx::SizeRectToAspectRatio(GetWindowResizeEdge(param), aspect_ratio_.value(),
                             min_window_size, max_window_size, window_rect);
}

// Removed unused GetCursorPos

}  // namespace ui
