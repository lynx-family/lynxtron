// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "plugin/cef/win/cef_webview_win.h"

#include <Windows.h>

#include <set>

// For Windows 7
static HHOOK gWinHook = nullptr;
static std::set<Win32PlatformView*> gWatchList;

namespace lynxtron {
namespace plugin {

void CEFWebviewWin::OnAttach() {
  if (wrapper_) {
    ::SetParent(wrapper_, lynx_view_->GetNativeWindow());
    ::ShowWindow(wrapper_, SW_SHOW);
  }
  if (win7_owned_win_) {
    ::ShowWindow(win7_owned_win_, SW_SHOW);
  }
  if (browser_) {
    CefRefPtr<CefBrowserHost> host = browser_->GetHost();
    if (hwnd_) {
      if (isWindows7) {
        ::SetWindowPos(win7_owned_win_, HWND_TOP, 0, 0, 0, 0,
                       SWP_NOSIZE | SWP_NOMOVE);
      } else {
        ::SetParent(hwnd_, lynx_view_->GetNativeWindow());
      }
      ::ShowWindow(hwnd_, SW_SHOW);
    }
    host->WasHidden(false);
  }
}

void CEFWebviewWin::OnDetach() {
  if (wrapper_) {
    ::ShowWindow(wrapper_, SW_HIDE);
  }
  if (win7_owned_win_) {
    ::ShowWindow(win7_owned_win_, SW_HIDE);
  }
  if (browser_) {
    CefRefPtr<CefBrowserHost> host = browser_->GetHost();
    if (hwnd_) {
      ::ShowWindow(hwnd_, SW_HIDE);
    }
    host->WasHidden(true);
  }
}

void CEFWebviewWin::OnDestroy() {
  if (browser_) {
    CefRefPtr<CefBrowserHost> host = browser_->GetHost();
    if (hwnd_) {
      ::SetParent(hwnd_, nullptr);
    }
    host->CloseBrowser(true);
  }
  browser_.reset();
  if (client_) {
    client_->webview_ = nullptr;
    client_ = nullptr;
  }

  if (wrapper_) {
    ::DestroyWindow(wrapper_);
    wrapper_ = nullptr;
  }
  auto search = gWatchList.find(this);
  if (search != gWatchList.end()) {
    gWatchList.erase(search);
  }
  if (win7_owned_win_) {
    ::DestroyWindow(win7_owned_win_);
    win7_owned_win_ = nullptr;
  }
}

void CEFWebviewWin::OnLayoutChanged(float left,
                                    float top,
                                    float width,
                                    float height,
                                    float pixel_ratio) {
  if (use_osr_) {
    RECT prev = bounds_;
    bounds_.left = LONG(left);
    bounds_.top = LONG(top);
    bounds_.right = LONG(left) + LONG(width);
    bounds_.bottom = LONG(top) + LONG(height);
    if (browser_ &&
        (prev.right != bounds_.right || prev.bottom != bounds_.bottom)) {
      CefRefPtr<CefBrowserHost> host = browser_->GetHost();
      // host->NotifyScreenInfoChanged();
      host->WasResized();
      host->Invalidate(PET_VIEW);
    }
  } else {
    bounds_ = RECT{LONG(left * pixel_ratio), LONG(top * pixel_ratio),
                   LONG((left + width) * pixel_ratio),
                   LONG((top + height) * pixel_ratio)};
    if (isWindows7) {
      AdjustOwnedWinAndChild(hwnd_);
    } else if (hwnd_) {
      ::SetWindowPos(hwnd_, nullptr, bounds_.left, bounds_.top,
                     bounds_.right - bounds_.left, bounds_.bottom - bounds_.top,
                     SWP_NOZORDER);
    }
  }
}

float CEFWebviewWin::GetPixelRatio() const {
  // TODO
  return 1.0f;
}

void CEFWebviewWin::SetupClient() {
  CefBrowserSettings settings;
  CefWindowInfo window_info;
  settings.chrome_status_bubble = STATE_DISABLED;
  // window_info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  if (use_osr_) {
    auto* native_window = lynx_view_get_native_window(lynx_view_);
    window_info.SetAsWindowless(reinterpret_cast<HWND>(native_window));
    window_info.shared_texture_enabled = 1;
    settings.windowless_frame_rate = fps_;
  } else if (isWindows7) {
    window_info.SetAsChild(GetOrCreateOwnedWin(),
                           CefRect(0, 0, bounds_.right - bounds_.left,
                                   bounds_.bottom - bounds_.top));
  } else {
    auto* native_window = lynx_view_get_native_window(lynx_view_);
    window_info.SetAsChild(
        reinterpret_cast<HWND>(native_window),
        CefRect(bounds_.left, bounds_.top, bounds_.right - bounds_.left,
                bounds_.bottom - bounds_.top));
  }
  client_ = new CEFWebviewClientWin(this);
  CefBrowserHost::CreateBrowser(window_info, client_, url_, settings, nullptr,
                                nullptr);
}

// private
HWND CEFWebviewWin::GetOrCreateOwnedWin() {
  if (!win7_owned_win_) {
    HWND parent = lynx_view_->GetNativeWindow();
    RECT rect = bounds_;
    ::ClientToScreen(parent, (LPPOINT)&rect.left);
    ::ClientToScreen(parent, (LPPOINT)&rect.right);

    win7_owned_win_ = ::CreateWindowEx(
        0, kClassName, 0,
        WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE, rect.left,
        rect.top, rect.right - rect.left, rect.bottom - rect.top,
        nullptr /*hWndParent */, nullptr, nullptr, nullptr);
    ::SetWindowLongPtr(win7_owned_win_, GWL_HWNDPARENT, LONG_PTR(parent));
    if (!gWinHook) {
      gWinHook = ::SetWindowsHookEx(WH_CALLWNDPROC, CallWndProc, nullptr,
                                    GetCurrentThreadId());
    }
    gWatchList.insert(this);
  }
  return win7_owned_win_;
}

void CEFWebviewWin::AdjustOwnedWinAndChild(HWND child) {
  if (win7_owned_win_) {
    HWND parent = lynx_view_->GetNativeWindow();
    RECT owned_rect = bounds_;
    RECT child_rect = {0, 0, bounds_.right - bounds_.left,
                       bounds_.bottom - bounds_.top};
    RECT client;
    ::GetClientRect(parent, &client);
    if (owned_rect.left < client.left) {
      child_rect.left -= client.left - owned_rect.left;
      owned_rect.left = client.left;
    }
    if (owned_rect.top < client.top) {
      child_rect.top -= client.top - owned_rect.top;
      owned_rect.top = client.top;
    }
    if (owned_rect.right > client.right) {
      owned_rect.right = client.right;
    }
    if (owned_rect.bottom > client.bottom) {
      owned_rect.bottom = client.bottom;
    }
    ::ClientToScreen(parent, (LPPOINT)&owned_rect.left);
    ::ClientToScreen(parent, (LPPOINT)&owned_rect.right);
    ::SetWindowPos(win7_owned_win_, nullptr, owned_rect.left, owned_rect.top,
                   owned_rect.right - owned_rect.left,
                   owned_rect.bottom - owned_rect.top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
    if (child) {
      ::SetWindowPos(child, nullptr, child_rect.left, child_rect.top,
                     child_rect.right - child_rect.left,
                     child_rect.bottom - child_rect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
  }
}

void CEFWebviewWin::OnMouseWheelEvent(int x,
                                      int y,
                                      int modifiers,
                                      double delta_x,
                                      double delta_y) {
  if (!browser_) {
    return;
  }
  if (CefRefPtr<CefBrowserHost> host = browser_->GetHost()) {
    CefMouseEvent mouse_event;
    mouse_event.x = x;
    mouse_event.y = y;
    mouse_event.modifiers = 0;
    if (modifiers & 1) {
      mouse_event.modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
    }
    if (modifiers & 2) {
      mouse_event.modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
    }
    if (modifiers & 4) {
      mouse_event.modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
    }
    if (::GetKeyState(VK_NUMLOCK) & 1) {
      mouse_event.modifiers |= EVENTFLAG_NUM_LOCK_ON;
    }
    if (::GetKeyState(VK_CAPITAL) & 1) {
      mouse_event.modifiers |= EVENTFLAG_CAPS_LOCK_ON;
    }
    host->SendMouseWheelEvent(mouse_event, delta_x, delta_y);
  }
}

void CEFWebviewWin::OnMouseMoveEvent(int x,
                                     int y,
                                     int modifiers,
                                     bool mouse_leave) {
  if (!browser_) {
    return;
  }
  if (CefRefPtr<CefBrowserHost> host = browser_->GetHost()) {
    CefMouseEvent mouse_event;
    mouse_event.x = x;
    mouse_event.y = y;
    mouse_event.modifiers = 0;
    if (modifiers & 1) {
      mouse_event.modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
    }
    if (modifiers & 2) {
      mouse_event.modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
    }
    if (modifiers & 4) {
      mouse_event.modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
    }
    if (::GetKeyState(VK_NUMLOCK) & 1) {
      mouse_event.modifiers |= EVENTFLAG_NUM_LOCK_ON;
    }
    if (::GetKeyState(VK_CAPITAL) & 1) {
      mouse_event.modifiers |= EVENTFLAG_CAPS_LOCK_ON;
    }
    host->SendMouseMoveEvent(mouse_event, mouse_leave);
  }
}

void CEFWebviewWin::OnMouseClickEvent(int x,
                                      int y,
                                      int buttons,
                                      bool mouse_up) {
  if (!browser_) {
    return;
  }

  if (!mouse_up) {
    LONG currentTime = GetMessageTime();
    if ((abs(last_click_x_ - x) > (GetSystemMetrics(SM_CXDOUBLECLK) / 2)) ||
        (abs(last_click_y_ - y) > (GetSystemMetrics(SM_CYDOUBLECLK) / 2)) ||
        ((currentTime - last_click_time_) > GetDoubleClickTime()) ||
        buttons != last_click_button_) {
      last_click_count_ = 1;
    } else {
      ++last_click_count_;
    }
    last_click_time_ = currentTime;
    last_click_button_ = buttons;
    last_click_x_ = x;
    last_click_y_ = y;
  }

  if (CefRefPtr<CefBrowserHost> host = browser_->GetHost()) {
    CefMouseEvent mouse_event;
    mouse_event.x = x;
    mouse_event.y = y;
    host->SendMouseClickEvent(
        mouse_event,
        buttons & 1
            ? MBT_LEFT
            : (buttons & 2 ? MBT_RIGHT : (buttons & 4 ? MBT_MIDDLE : MBT_LEFT)),
        mouse_up, last_click_count_);
  }
}

}  // namespace plugin
}  // namespace lynxtron

LYNX_EXTERN_C lynx_native_view_t* cef_webview_create_view(void* opaque) {
  lynxtron::plugin::CEFWebviewWin* view =
      new lynxtron::plugin::CEFWebviewWin(static_cast<lynx_view_t*>(opaque));
  return view->native_view();
}
