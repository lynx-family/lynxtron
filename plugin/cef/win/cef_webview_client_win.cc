// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "plugin/cef/win/cef_webview_client_win.h"

#include <algorithm>

namespace lynxtron {
namespace plugin {

CEFWebviewClientWin::CEFWebviewClientWin(CEFWebviewWin* webview)
    : CEFWebviewClient(webview) {
  parent_ = reinterpret_cast<HWND>(
      lynx_view_get_native_window(webview->GetLynxView()));
}

void CEFWebviewClientWin::GetViewRect(CefRefPtr<CefBrowser> browser,
                                      CefRect& rect) {
  if (!webview_) {
    return;
  }
  auto& bounds = webview_->bounds_;
  rect.Set(0, 0, std::max(1L, bounds.right - bounds.left),
           std::max(1L, bounds.bottom - bounds.top));
}

bool CEFWebviewClientWin::GetScreenPoint(CefRefPtr<CefBrowser> browser,
                                         int viewX,
                                         int viewY,
                                         int& screenX,
                                         int& screenY) {
  float ratio = webview_->GetPixelRatio();
  POINT screen_pt = {LONG(viewX * ratio), LONG(viewY * ratio)};
  ::ClientToScreen(parent_, &screen_pt);
  screenX = screen_pt.x;
  screenY = screen_pt.y;
  return true;
}

void CEFWebviewClientWin::OnAcceleratedPaint(
    CefRefPtr<CefBrowser> browser,
    PaintElementType type,
    const RectList& dirtyRects,
    const CefAcceleratedPaintInfo& info) {
  // TODO: check image sink
  static const float transform[3 * 3] = {1, 0, 0, 0, -1, 1, 0, 0, 1};
  webview_->PresentSurface(
      info.extra.source_size.width, info.extra.source_size.height, transform,
      reinterpret_cast<lynx_surface_handle_t*>(info.shared_texture_io_surface));
}

void CEFWebviewClientWin::OnImeCompositionRangeChanged(
    CefRefPtr<CefBrowser> browser,
    const CefRange& selection_range,
    const CefRenderHandler::RectList& character_bounds) {
  // if (ime_handler_) {
  //   float ratio = webview_->GetPixelRatio();
  //   auto& bounds = webview_->bounds_;
  //   CefRenderHandler::RectList device_bounds;
  //   CefRenderHandler::RectList::const_iterator it = character_bounds.begin();
  //   for (; it != character_bounds.end(); ++it) {
  //     CefRect rect((bounds.left + it->x) * ratio, (bounds.top + it->y) *
  //     ratio,
  //                  it->width * ratio, it->height * ratio);
  //     device_bounds.push_back(rect);
  //   }

  //   ime_handler_->ChangeCompositionRange(selection_range, device_bounds);
  // }
}

void CEFWebviewClientWin::OnVirtualKeyboardRequested(
    CefRefPtr<CefBrowser> browser,
    TextInputMode input_mode) {
  switch (input_mode) {
    case CEF_TEXT_INPUT_MODE_NONE:
      if (!ime_shown_) {
        return;
      }
      ime_shown_ = false;
      PostMessageW(parent_, kRequestIME, 0, 0);
      break;
    default:
      if (ime_shown_) {
        return;
      }
      ime_shown_ = true;
      PostMessageW(parent_, kRequestIME, reinterpret_cast<WPARAM>(ime_callback),
                   reinterpret_cast<LPARAM>(this));
      webview_->RequestFocus();
      break;
  }
}

bool OnCursorChange(CefRefPtr<CefBrowser> browser,
                    CefCursorHandle cursor,
                    cef_cursor_type_t type,
                    const CefCursorInfo& custom_cursor_info) {
  if (webview_ && webview_->UseOSR()) {
    // Change the window's cursor.
    SetClassLongPtr(parent_, GCLP_HCURSOR,
                    static_cast<LONG>(reinterpret_cast<LONG_PTR>(cursor)));
    SetCursor(cursor);
    return true;
  }
  return false;
}

void CEFWebviewClientWin::AttachToView(CefRefPtr<CefBrowser> browser) {
  CefRefPtr<CefBrowserHost> host = browser->GetHost();
  HWND hwnd = host->GetWindowHandle();
  webview_->hwnd_ = hwnd;

  if (isWindows7) {
    webview_->AdjustOwnedWinAndChild(hwnd);
  } else {
    auto& bounds = static_cast<CEFWebviewWin*>(webview_)->bounds_;
    ::SetWindowPos(hwnd, nullptr, bounds.left, bounds.top,
                   bounds.right - bounds.left, bounds.bottom - bounds.top,
                   SWP_NOZORDER);
  }
}

}  // namespace plugin
}  // namespace lynxtron
