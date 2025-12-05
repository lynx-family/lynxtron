// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "plugin/cef/darwin/macos/CEFWebviewClientMac.h"

#include "plugin/cef/darwin/macos/CEFWebviewMac.h"

namespace lynxtron {
namespace plugin {

void CEFWebviewClientMac::GetViewRect(CefRefPtr<CefBrowser> browser,
                                      CefRect& rect) {
  if (!webview_) {
    return;
  }
  auto& bounds = static_cast<CEFWebviewMac*>(webview_)->bounds_;
  rect.Set(0, 0, std::max(1L, long(bounds.size.width)),
           std::max(1L, long(bounds.size.height)));
}

bool CEFWebviewClientMac::GetScreenInfo(CefRefPtr<CefBrowser> browser,
                                        CefScreenInfo& screen_info) {
  float ratio = webview_->GetPixelRatio();
  CefRect view_rect;
  GetViewRect(browser, view_rect);
  screen_info.device_scale_factor = ratio;
  screen_info.rect = view_rect;
  screen_info.available_rect = view_rect;
  return true;
}

bool CEFWebviewClientMac::GetScreenPoint(CefRefPtr<CefBrowser> browser,
                                         int viewX,
                                         int viewY,
                                         int& screenX,
                                         int& screenY) {
  NSPoint view_pt = {static_cast<CGFloat>(viewX), static_cast<CGFloat>(viewY)};
  NSView* view =
      (__bridge NSView*)(lynx_view_get_native_window(webview_->GetLynxView()));
  NSPoint window_pt = [view convertPoint:view_pt toView:nil];
  NSPoint screen_pt = [[view window] convertPointToScreen:window_pt];
  screenX = screen_pt.x;
  screenY = screen_pt.y;
  return true;
}

void CEFWebviewClientMac::OnAcceleratedPaint(
    CefRefPtr<CefBrowser> browser,
    PaintElementType type,
    const RectList& dirtyRects,
    const CefAcceleratedPaintInfo& info) {
  float ratio = webview_->GetPixelRatio();
  auto& bounds = static_cast<CEFWebviewMac*>(webview_)->bounds_;
  if (info.extra.source_size.width != int(bounds.size.width * ratio) ||
      info.extra.source_size.height != int(bounds.size.height * ratio)) {
    return;
  }
  // TODO: check image sink
  static const float transform[3 * 3] = {1, 0, 0, 0, -1, 1, 0, 0, 1};
  webview_->PresentSurface(
      info.extra.source_size.width, info.extra.source_size.height, transform,
      reinterpret_cast<lynx_surface_handle_t*>(info.shared_texture_io_surface));
}

void CEFWebviewClientMac::OnImeCompositionRangeChanged(
    CefRefPtr<CefBrowser> browser,
    const CefRange& selection_range,
    const CefRenderHandler::RectList& character_bounds) {
  auto& bounds = static_cast<CEFWebviewMac*>(webview_)->bounds_;
  CefRenderHandler::RectList device_bounds;
  for (auto it = character_bounds.begin(); it != character_bounds.end(); ++it) {
    CefRect rect(bounds.origin.x + it->x, bounds.origin.y + it->y, it->width,
                 it->height);
    device_bounds.push_back(rect);
  }
  [text_input_client_ ChangeCompositionRange:selection_range
                            character_bounds:device_bounds];
}

void CEFWebviewClientMac::OnVirtualKeyboardRequested(
    CefRefPtr<CefBrowser> browser,
    TextInputMode input_mode) {
  switch (input_mode) {
    case CEF_TEXT_INPUT_MODE_NONE:
      // if (!ime_shown_) {
      //   return;
      // }
      // //[text_input_context_osr_mac_ deactivate];
      // [webview_->lynx_view_ requestIME:nil arg:nil];
      // text_input_context_osr_mac_ = nullptr;
      // text_input_client_ = nullptr;
      // ime_shown_ = false;
      break;
    default:
      // if (ime_shown_) {
      //   return;
      // }
      // ime_shown_ = true;
      // text_input_client_ =
      //     [[CefTextInputClientOSRMac alloc]
      //     initWithBrowser:webview_->browser_];
      // text_input_context_osr_mac_ =
      //     [[NSTextInputContext alloc] initWithClient:text_input_client_];
      // webview_->RequestFocus();
      // [webview_->lynx_view_ requestIME:(void*)ime_callback arg:this];
      // [text_input_context_osr_mac_ activate];
      break;
  }
}

bool CEFWebviewClientMac::OnCursorChange(
    CefRefPtr<CefBrowser> browser,
    CefCursorHandle cursor,
    cef_cursor_type_t type,
    const CefCursorInfo& custom_cursor_info) {
  if (webview_ && webview_->UseOSR()) {
    [CAST_CEF_CURSOR_HANDLE_TO_NSCURSOR(cursor) set];
    if (type == CT_NONE) {
      [NSCursor unhide];
    }
    return true;
  }
  return false;
}

void CEFWebviewClientMac::AttachToView(CefRefPtr<CefBrowser> browser) {
  CefRefPtr<CefBrowserHost> host = browser->GetHost();
  cef_window_handle_t native_view = host->GetWindowHandle();
  webview_->SetNativeView(native_view);

  NSView* nsView = CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(native_view);
  nsView.autoresizingMask = NSViewNotSizable;
  nsView.frame = static_cast<CEFWebviewMac*>(webview_)->bounds_;

  if (!static_cast<CEFWebviewMac*>(webview_)->IsAttached()) {
    static_cast<CEFWebviewMac*>(webview_)->AddSubview(nsView);
  }
}

}  // namespace plugin
}  // namespace lynxtron
