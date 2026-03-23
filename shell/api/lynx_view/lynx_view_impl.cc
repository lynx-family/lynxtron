// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/lynx_view_impl.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "api/lynx_view/lynx_view_client.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "gfx/geometry/rect.h"
#include "platform/embedder/public/lynx_load_meta.h"
#include "platform/embedder/public/lynx_template_data.h"
#include "platform/embedder/public/lynx_update_meta.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <windowsx.h>
#endif

#include "build/build_config.h"
#include "lynx/platform/embedder/public/lynx_view.h"
#include "shell/api/lynx_view/module/lynx_bridge_module.h"
#include "shell/api/lynx_view/module/lynx_node_module.h"
#include "shell/lynx/resource_fetcher/lynx_generic_resource_fetcher_factory.h"

namespace {

#if BUILDFLAG(IS_WIN)
constexpr wchar_t kLynxViewHitTestOldWndProcProp[] =
    L"LynxtronLynxViewHitTestOldWndProc";

using GetDpiForWindowPtr = decltype(::GetDpiForWindow)*;
using GetSystemMetricsForDpiPtr = decltype(::GetSystemMetricsForDpi)*;

void* GetUser32FunctionPointer(const char* function_name) {
  static HMODULE user32_module = ::GetModuleHandleW(L"user32.dll");
  if (!user32_module) {
    user32_module = ::LoadLibraryW(L"user32.dll");
  }
  return user32_module ? reinterpret_cast<void*>(
                             ::GetProcAddress(user32_module, function_name))
                       : nullptr;
}

int GetFrameThicknessForDpi(UINT dpi) {
  static const auto get_system_metrics_for_dpi_func =
      reinterpret_cast<GetSystemMetricsForDpiPtr>(
          GetUser32FunctionPointer("GetSystemMetricsForDpi"));
  if (get_system_metrics_for_dpi_func) {
    return get_system_metrics_for_dpi_func(SM_CXSIZEFRAME, dpi) +
           get_system_metrics_for_dpi_func(SM_CXPADDEDBORDER, dpi);
  }
  return ::GetSystemMetrics(SM_CXSIZEFRAME) +
         ::GetSystemMetrics(SM_CXPADDEDBORDER);
}

int GetFrameThicknessForHwnd(HWND hwnd) {
  static const auto get_dpi_for_window_func =
      reinterpret_cast<GetDpiForWindowPtr>(
          GetUser32FunctionPointer("GetDpiForWindow"));
  const UINT dpi = get_dpi_for_window_func ? get_dpi_for_window_func(hwnd) : 96;
  return GetFrameThicknessForDpi(dpi);
}

LRESULT CALLBACK LynxViewHitTestWndProc(HWND hwnd,
                                        UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam) {
  auto old_proc = reinterpret_cast<WNDPROC>(
      ::GetPropW(hwnd, kLynxViewHitTestOldWndProcProp));
  if (!old_proc) {
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
  }

  if (message == WM_NCHITTEST) {
    LRESULT hit = ::CallWindowProcW(old_proc, hwnd, message, wparam, lparam);
    if (hit == HTCLIENT) {
      const POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      RECT rect{};
      if (::GetWindowRect(hwnd, &rect)) {
        const int thickness = std::max(1, GetFrameThicknessForHwnd(hwnd));
        RECT inner = rect;
        ::InflateRect(&inner, -thickness, -thickness);
        if (!::PtInRect(&inner, pt)) {
          return HTTRANSPARENT;
        }
      }
    }
    return hit;
  }

  if (message == WM_NCDESTROY) {
    ::RemovePropW(hwnd, kLynxViewHitTestOldWndProcProp);
    ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                        reinterpret_cast<LONG_PTR>(old_proc));
    return ::CallWindowProcW(old_proc, hwnd, message, wparam, lparam);
  }

  return ::CallWindowProcW(old_proc, hwnd, message, wparam, lparam);
}

void InstallLynxViewHitTestHook(HWND hwnd) {
  if (!::IsWindow(hwnd)) {
    return;
  }
  // Only enable this for frameless windows (top-level window without
  // WS_CAPTION). In frameless mode the top-level window typically implements
  // resize hit-testing in WM_NCHITTEST. If the mouse is over the Lynx child
  // HWND edge and the child returns HTCLIENT, hit-testing won't propagate to
  // the parent, breaking resize/drag behaviors.
  HWND top_level = ::GetAncestor(hwnd, GA_ROOT);
  if (::IsWindow(top_level)) {
    const LONG style = ::GetWindowLongW(top_level, GWL_STYLE);
    if (style & WS_CAPTION) {
      return;
    }
  }
  if (::GetPropW(hwnd, kLynxViewHitTestOldWndProcProp)) {
    return;
  }

  ::SetLastError(0);
  auto old_proc = reinterpret_cast<WNDPROC>(::SetWindowLongPtrW(
      hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&LynxViewHitTestWndProc)));
  if (!old_proc && ::GetLastError() != 0) {
    return;
  }
  ::SetPropW(hwnd, kLynxViewHitTestOldWndProcProp,
             reinterpret_cast<HANDLE>(old_proc));
}
#endif

}  // namespace

namespace lynxtron {

LynxViewImpl::~LynxViewImpl() {
  Close();
}

void LynxViewImpl::Init(
    double width,
    double height,
    float dpi,
    void* parent,
    const std::vector<std::string>& node_integration_preload,
    base::WeakPtr<api::LynxWindow> lynx_window) {
  lynx::pub::LynxView::Builder builder;
  builder.SetScreenSize(width, height, dpi)
      .SetFrame(0, 0, width, height)
      .SetParent(parent);
  // .SetGenericResourceFetcher(
  //     LynxGenericResourceFetcherFactory::Create(lynx_window));

  if (!node_integration_preload.empty()) {
    RegisterLynxNodeModuleToLynxView(builder.Impl(), node_integration_preload);
  }
  RegisterLynxBridgeModuleToLynxView(builder.Impl(), lynx_window);

  lynx_view_ = builder.Build();
#if BUILDFLAG(IS_WIN)
  // Hook the Lynx render child HWND: return HTTRANSPARENT near the edge so
  // hit-testing continues to the parent (which handles resize/caption logic).
  InstallLynxViewHitTestHook(
      reinterpret_cast<HWND>(lynx_view_->GetNativeWindow()));
#endif
  // Create a shared_ptr alias that doesn't manage the memory
  // This is safe as long as lynx_view_ doesn't outlive LynxViewImpl
  lynx_view_->AddClient(
      std::shared_ptr<LynxViewImpl>(std::shared_ptr<LynxViewImpl>(), this));
}

void LynxViewImpl::LoadTemplate(std::string_view template_url,
                                base::span<const uint8_t> content) {
  auto meta_data = std::make_shared<lynx::pub::LynxLoadMeta>();
  meta_data->SetUrl(std::string(template_url));

  meta_data->SetBinaryData(const_cast<uint8_t*>(content.data()),
                           content.size());
  lynx_view_->LoadTemplate(meta_data);
}

void LynxViewImpl::SetClient(base::WeakPtr<lynxtron::LynxViewClient> client) {
  lynx_view_client_ = client;
}

void LynxViewImpl::SetBounds(const gfx::Rect& bounds) {
  lynx_view_->SetFrame(bounds.x(), bounds.y(), bounds.width(), bounds.height());
}

void LynxViewImpl::SendGlobalEvent(const std::string& event,
                                   const std::string& json) {
  lynx_view_->SendGlobalEvent(event, json);
}

void LynxViewImpl::ReloadTemplate(const std::string& data,
                                  const std::string& global_props) {
  lynx_view_->ReloadTemplate(
      std::make_shared<lynx::pub::LynxTemplateData>(data),
      std::make_shared<lynx::pub::LynxTemplateData>(global_props));
}

void LynxViewImpl::UpdateData(const std::string& data,
                              const std::string& global_props) {
  auto meta_data = std::make_shared<lynx::pub::LynxUpdateMeta>();
  meta_data->SetUpdateData(std::make_shared<lynx::pub::LynxTemplateData>(data));
  meta_data->SetGlobalProps(
      std::make_shared<lynx::pub::LynxTemplateData>(global_props));
  lynx_view_->UpdateData(meta_data);
}

void LynxViewImpl::UpdateScreenMetrics(float width,
                                       float height,
                                       float device_pixel_ratio) {
  lynx_view_->UpdateScreenMetrics(width, height, device_pixel_ratio);
}

void LynxViewImpl::SetFrame(float x, float y, float width, float height) {
  lynx_view_->SetFrame(x, y, width, height);
}

void* LynxViewImpl::GetNativeWindow() {
  return lynx_view_ ? lynx_view_->GetNativeWindow() : nullptr;
}

void LynxViewImpl::Show() {}

void LynxViewImpl::Hide() {}

void LynxViewImpl::Close() {
  if (lynx_view_) {
    lynx_view_->RemoveClient(
        std::shared_ptr<LynxViewImpl>(std::shared_ptr<LynxViewImpl>(), this));
    lynx_view_.reset();
  }
}

void LynxViewImpl::OnPageStart(const char* url) {
  if (lynx_view_client_) {
    lynx_view_client_->OnPageStart(url);
  }
}

void LynxViewImpl::OnLoadSuccess() {
  if (lynx_view_client_) {
    lynx_view_client_->OnLoadSuccess();
  }
}

void LynxViewImpl::OnFirstScreen() {
  if (lynx_view_client_) {
    lynx_view_client_->OnFirstScreen();
  }
}

void LynxViewImpl::OnPageUpdated() {
  if (lynx_view_client_) {
    lynx_view_client_->OnPageUpdated();
  }
}

void LynxViewImpl::OnDataUpdated() {
  if (lynx_view_client_) {
    lynx_view_client_->OnDataUpdated();
  }
}

void LynxViewImpl::OnDestroy() {
  if (lynx_view_client_) {
    lynx_view_client_->OnDestroy();
  }
}

void LynxViewImpl::OnRuntimeReady() {
  if (lynx_view_client_) {
    lynx_view_client_->OnRuntimeReady();
  }
}

void LynxViewImpl::OnReceivedError(int error_code, const char* message) {
  if (lynx_view_client_) {
    lynx_view_client_->OnReceivedError(error_code, message);
  }
}

void LynxViewImpl::OnTimingSetup(const char* timing_info) {
  if (lynx_view_client_) {
    lynx_view_client_->OnTimingSetup(timing_info);
  }
}

void LynxViewImpl::OnTimingUpdate(const char* timing_info,
                                  const char* update_timing,
                                  const char* update_flag) {
  if (lynx_view_client_) {
    lynx_view_client_->OnTimingUpdate(timing_info, update_timing, update_flag);
  }
}

void LynxViewImpl::OnEnterForeground() {
  if (lynx_view_client_) {
    lynx_view_client_->OnEnterForeground();
  }
}

void LynxViewImpl::OnEnterBackground() {
  if (lynx_view_client_) {
    lynx_view_client_->OnEnterBackground();
  }
}

}  // namespace lynxtron
