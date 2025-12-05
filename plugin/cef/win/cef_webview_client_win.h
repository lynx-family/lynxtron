// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef PLUGIN_CEF_WIN_CEF_WEBVIEW_CLIENT_WIN_H_
#define PLUGIN_CEF_WIN_CEF_WEBVIEW_CLIENT_WIN_H_

#include "plugin/cef/cef_webview_client.h"

namespace lynxtron {
namespace plugin {

class CEFWebviewWin;

class CEFWebviewClientWin : public CEFWebviewClient {
 public:
  CEFWebviewClientWin(CEFWebviewWin* webview);

  // CefRenderHandler
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;

  bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                      int viewX,
                      int viewY,
                      int& screenX,
                      int& screenY) override;

  void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                          PaintElementType type,
                          const RectList& dirtyRects,
                          const CefAcceleratedPaintInfo& info) override;

  void OnImeCompositionRangeChanged(
      CefRefPtr<CefBrowser> browser,
      const CefRange& selection_range,
      const CefRenderHandler::RectList& character_bounds) override;

  void OnVirtualKeyboardRequested(CefRefPtr<CefBrowser> browser,
                                  TextInputMode input_mode) override;

  // CefDisplayHandler methods.
  bool OnCursorChange(CefRefPtr<CefBrowser> browser,
                      CefCursorHandle cursor,
                      cef_cursor_type_t type,
                      const CefCursorInfo& custom_cursor_info) override;

  void AttachToView(CefRefPtr<CefBrowser> browser) override;

 private:
  HWND parent_;
};

}  // namespace plugin
}  // namespace lynxtron

#endif  // PLUGIN_CEF_WIN_CEF_WEBVIEW_CLIENT_WIN_H_
