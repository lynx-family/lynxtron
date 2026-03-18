// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/lynx_view.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "shell/api/lynx_view/lynx_view_client.h"
#include "shell/lynx/resource_fetcher/lynx_generic_resource_fetcher_factory.h"

#if BUILDFLAG(IS_MAC)
#include "lynx/platform/embedder/public/capi/lynx_env_capi.h"
#include "lynx/platform/embedder/public/lynx_view.h"
#include "lynx/platform/embedder/public/lynx_view_client.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "lynx/platform/embedder/public/capi/lynx_env_capi.h"
#include "lynx/platform/embedder/public/lynx_view.h"
#include "lynx/platform/embedder/public/lynx_view_client.h"
#endif

#include "shell/api/lynx_view/module/lynx_bridge_module.h"
#include "shell/api/lynx_view/module/lynx_node_module.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <windowsx.h>
#endif

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

class LynxViewImpl : public lynx::pub::LynxViewClient,
                     public std::enable_shared_from_this<LynxViewImpl> {
 public:
  LynxViewImpl() = default;
  ~LynxViewImpl() = default;

  void Init(double width,
            double height,
            float dpi,
            void* parent,
            const std::vector<std::string>& node_integration_preload,
            base::WeakPtr<api::LynxWindow> lynx_window) {
    lynx::pub::LynxView::Builder builder;
    builder.SetScreenSize(width, height, dpi)
        .SetFrame(0, 0, width, height)
        .SetParent(parent)
        .SetGenericResourceFetcher(
            LynxGenericResourceFetcherFactory::Create(lynx_window));

    if (!node_integration_preload.empty()) {
      RegisterLynxNodeModuleToLynxView(builder.Impl(),
                                       node_integration_preload);
    }
    RegisterLynxBridgeModuleToLynxView(builder.Impl(), lynx_window);

    lynx_view_ = builder.Build();
#if BUILDFLAG(IS_WIN)
    // Hook the Lynx render child HWND: return HTTRANSPARENT near the edge so
    // hit-testing continues to the parent (which handles resize/caption logic).
    InstallLynxViewHitTestHook(
        reinterpret_cast<HWND>(lynx_view_->GetNativeWindow()));
#endif
    lynx_view_->AddClient(shared_from_this());
  }

  void LoadTemplate(std::string_view template_url,
                    base::span<uint8_t> content) {
    auto meta_data = std::make_shared<lynx::pub::LynxLoadMeta>();
    meta_data->SetUrl(std::string(template_url));

    meta_data->SetBinaryData(content.data(), content.size());
    lynx_view_->LoadTemplate(meta_data);
  }

  void SetClient(base::WeakPtr<lynxtron::LynxViewClient> client) {
    lynx_view_client_ = client;
  }

  void SetBounds(const gfx::Rect& bounds) {
    lynx_view_->SetFrame(bounds.x(), bounds.y(), bounds.width(),
                         bounds.height());
  }

  void SendGlobalEvent(const std::string& event, const std::string& json) {
    lynx_view_->SendGlobalEvent(event, json);
  }

  void ReloadTemplate(const std::string& data,
                      const std::string& global_props) {
    lynx_view_->ReloadTemplate(
        std::make_shared<lynx::pub::LynxTemplateData>(data),
        std::make_shared<lynx::pub::LynxTemplateData>(global_props));
  }

  void UpdateData(const std::string& data, const std::string& global_props) {
    auto meta_data = std::make_shared<lynx::pub::LynxUpdateMeta>();
    meta_data->SetUpdateData(
        std::make_shared<lynx::pub::LynxTemplateData>(data));
    meta_data->SetGlobalProps(
        std::make_shared<lynx::pub::LynxTemplateData>(global_props));
    lynx_view_->UpdateData(meta_data);
  }

  void UpdateScreenMetrics(float width,
                           float height,
                           float device_pixel_ratio) {
    lynx_view_->UpdateScreenMetrics(width, height, device_pixel_ratio);
  }

  void SetFrame(float x, float y, float width, float height) {
    lynx_view_->SetFrame(x, y, width, height);
  }

  void* GetNativeWindow() {
    return lynx_view_ ? lynx_view_->GetNativeWindow() : nullptr;
  }

  void Show() {}

  void Hide() {}

  void Close() { lynx_view_.reset(); }

  // TODO(Guo Xi): check lynx callback thread
  // lynx::pub::LynxViewClient
  void OnPageStart(const char* url) override {
    if (lynx_view_client_) {
      lynx_view_client_->OnPageStart(url);
    }
  }
  void OnLoadSuccess() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnLoadSuccess();
    }
  }
  void OnFirstScreen() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnFirstScreen();
    }
  }
  void OnPageUpdated() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnPageUpdated();
    }
  }
  void OnDataUpdated() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnDataUpdated();
    }
  }
  void OnDestroy() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnDestroy();
    }
  }
  void OnRuntimeReady() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnRuntimeReady();
    }
  }
  void OnReceivedError(int error_code, const char* message) override {
    if (lynx_view_client_) {
      lynx_view_client_->OnReceivedError(error_code, message);
    }
  }
  void OnTimingSetup(const char* timing_info) override {
    if (lynx_view_client_) {
      lynx_view_client_->OnTimingSetup(timing_info);
    }
  }
  void OnTimingUpdate(const char* timing_info,
                      const char* update_timing,
                      const char* update_flag) override {
    if (lynx_view_client_) {
      lynx_view_client_->OnTimingUpdate(timing_info, update_timing,
                                        update_flag);
    }
  }
  void OnEnterForeground() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnEnterForeground();
    }
  }
  void OnEnterBackground() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnEnterBackground();
    }
  }

 private:
  std::unique_ptr<lynx::pub::LynxView> lynx_view_;
  base::WeakPtr<lynxtron::LynxViewClient> lynx_view_client_;
};

LynxView::LynxView(base::WeakPtr<api::LynxWindow> lynx_window)
    : lynx_window_(lynx_window), impl_(std::make_shared<LynxViewImpl>()) {}

LynxView::~LynxView() = default;

std::unique_ptr<LynxView> LynxView::Create(
    base::WeakPtr<api::LynxWindow> lynx_window) {
  return base::WrapUnique(new LynxView(lynx_window));
}

// static
void LynxView::SetNodePlatformEnv(void* platform) {
  lynx_env_set_node_platform(platform);
  SetNodePlatformEnvToLynxNodeModule(platform);
}

void LynxView::Init(double width,
                    double height,
                    float dpi,
                    void* parent,
                    const std::vector<std::string>& node_integration_preload) {
  impl_->Init(width, height, dpi, parent, node_integration_preload,
              lynx_window_);
}

void LynxView::LoadTemplate(std::string_view template_url,
                            base::span<uint8_t> content) {
  impl_->LoadTemplate(template_url, content);
}

void LynxView::SetClient(base::WeakPtr<lynxtron::LynxViewClient> client) {
  impl_->SetClient(client);
}

void LynxView::SetBounds(const gfx::Rect& bounds) {
  impl_->SetBounds(bounds);
}

void LynxView::Show() {
  impl_->Show();
}

void LynxView::Hide() {
  impl_->Hide();
}

void LynxView::Close() {
  impl_->Close();
}

void LynxView::SendGlobalEvent(const std::string& event,
                               const std::string& json) {
  impl_->SendGlobalEvent(event, json);
}

void LynxView::UpdateData(const std::string& data,
                          const std::string& global_props) {
  impl_->UpdateData(data, global_props);
}

void LynxView::ReloadTemplate(const std::string& data,
                              const std::string& global_props) {
  impl_->ReloadTemplate(data, global_props);
}

void LynxView::UpdateScreenMetrics(float width,
                                   float height,
                                   float device_pixel_ratio) {
  impl_->UpdateScreenMetrics(width, height, device_pixel_ratio);
}

void LynxView::SetFrame(float x, float y, float width, float height) {
  impl_->SetFrame(x, y, width, height);
}

void* LynxView::GetNativeWindow() {
  return impl_->GetNativeWindow();
}

}  // namespace lynxtron
