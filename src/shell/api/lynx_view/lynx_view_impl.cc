// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/lynx_view_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "lynx/platform/embedder/public/capi/lynx_view_builder_capi.h"
#include "lynx/platform/embedder/public/lynx_load_meta.h"
#include "lynx/platform/embedder/public/lynx_template_bundle.h"
#include "lynx/platform/embedder/public/lynx_view.h"
#include "platform/embedder/public/lynx_template_data.h"
#include "platform/embedder/public/lynx_update_meta.h"
#include "shell/api/lynx_view/devtool_event_simulation_proxy.h"
#include "shell/api/lynx_view/lynx_update_meta.h"
#include "shell/api/lynx_view/lynx_view_builder.h"
#include "shell/api/lynx_view/lynx_view_client.h"
#include "shell/api/lynx_view/module/lynx_bridge_module.h"
#include "shell/api/lynx_view/module/lynx_hybrid_monitor_module.h"
#include "shell/api/lynx_view/module/lynx_node_module.h"
#include "shell/common/asar/archive.h"
#include "shell/common/asar/asar_util.h"
#include "shell/common/global_thread.h"
#include "shell/common/thread_restrictions.h"
#include "shell/lynx/resource_fetcher/lynx_generic_resource_fetcher_factory.h"
#include "shell/ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <windowsx.h>

#include "shell/ui/gfx/win/hwnd_util.h"
#endif

namespace {

std::vector<uint8_t> LoadFileData(std::string_view path) {
  base::FilePath in_file_name = base::FilePath::FromUTF8Unsafe(path);
  std::string file_contents;
  {
    lynxtron::ScopedAllowBlockingForLynxtron allow_blocking;
    if (!asar::ReadFileToString(in_file_name, &file_contents)) {
      return {};
    }
  }
  size_t size = file_contents.size();
  std::vector<uint8_t> buf(file_contents.data(), file_contents.data() + size);
  return buf;
}

std::string ToFileUrl(const base::FilePath& path) {
  std::string normalized = path.AsUTF8Unsafe();
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
#if BUILDFLAG(IS_WIN)
  return "file:///" + normalized;
#else
  return "file://" + normalized;
#endif
}

#if BUILDFLAG(IS_WIN)
constexpr wchar_t kLynxViewHitTestOldWndProcProp[] =
    L"LynxtronLynxViewHitTestOldWndProc";

bool IsResizeHitTest(LRESULT hit) {
  switch (hit) {
    case HTLEFT:
    case HTRIGHT:
    case HTTOP:
    case HTTOPLEFT:
    case HTTOPRIGHT:
    case HTBOTTOM:
    case HTBOTTOMLEFT:
    case HTBOTTOMRIGHT:
      return true;
    default:
      return false;
  }
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
      HWND top_level = ::GetAncestor(hwnd, GA_ROOT);
      if (::IsWindow(top_level) && top_level != hwnd) {
        const LRESULT top_level_hit =
            ::SendMessageW(top_level, WM_NCHITTEST, wparam, lparam);
        if (IsResizeHitTest(top_level_hit)) {
          return HTTRANSPARENT;
        }
      }
    }
    return hit;
  }

  if (message == WM_NCDESTROY) {
    ::RemovePropW(hwnd, kLynxViewHitTestOldWndProcProp);
    gfx::SetWindowProc(hwnd, old_proc);
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

  auto old_proc = gfx::SetWindowProc(hwnd, &LynxViewHitTestWndProc);
  if (!old_proc) {
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

void LynxViewImpl::Initialize(std::unique_ptr<lynx::pub::LynxView> core_view) {
  lynx_view_ = std::move(core_view);
  lynx_view_->SetEventSimulationProxy(
      std::make_shared<internal::DevtoolEventSimulationProxy>(
          std::make_unique<internal::LynxViewEventTarget>(lynx_view_.get())));
#if BUILDFLAG(IS_WIN)
  // Hook the Lynx render child HWND: return HTTRANSPARENT near the edge so
  // hit-testing continues to the parent (which handles resize/caption logic).
  InstallLynxViewHitTestHook(
      reinterpret_cast<HWND>(lynx_view_->GetNativeWindow()));
#endif
  // Create a shared_ptr with a no-op deleter to provide a valid control block
  // without managing the memory of 'this', and keep it alive as a member.
  self_shared_ptr_ = std::shared_ptr<LynxViewImpl>(this, [](LynxViewImpl*) {});
  lynx_view_->AddClient(self_shared_ptr_);
}

void LynxViewImpl::LoadFile(const std::string& path,
                            const std::string& data,
                            const std::string& global_props) {
  auto meta = std::make_shared<lynx::pub::LynxLoadMeta>();
  base::FilePath local_path = base::FilePath::FromUTF8Unsafe(path);
  auto source = LoadFileData(path);
  if (source.empty()) {
    return;
  }

  meta->SetUrl(ToFileUrl(local_path));
  meta->SetBinaryData(source);
  if (!data.empty()) {
    meta->SetInitialData(std::make_shared<lynx::pub::LynxTemplateData>(data));
  }
  if (!global_props.empty()) {
    meta->SetGlobalProps(
        std::make_shared<lynx::pub::LynxTemplateData>(global_props));
  }
  lynx_view_->LoadTemplate(std::move(meta));
}

void LynxViewImpl::LoadURL(const std::string& url,
                           const std::string& data,
                           const std::string& global_props) {
  auto meta = std::make_shared<lynx::pub::LynxLoadMeta>();
  meta->SetUrl(url);
  if (!data.empty()) {
    meta->SetInitialData(std::make_shared<lynx::pub::LynxTemplateData>(data));
  }
  if (!global_props.empty()) {
    meta->SetGlobalProps(
        std::make_shared<lynx::pub::LynxTemplateData>(global_props));
  }
  lynx_view_->LoadTemplate(std::move(meta));
}

void LynxViewImpl::LoadBundle(
    std::shared_ptr<lynx::pub::LynxTemplateBundle> bundle,
    const std::string& data,
    const std::string& global_props) {
  auto meta = std::make_shared<lynx::pub::LynxLoadMeta>();
  meta->SetUrl("bundle://local");
  meta->SetTemplateBundle(std::move(bundle));
  if (!data.empty()) {
    meta->SetInitialData(std::make_shared<lynx::pub::LynxTemplateData>(data));
  }
  if (!global_props.empty()) {
    meta->SetGlobalProps(
        std::make_shared<lynx::pub::LynxTemplateData>(global_props));
  }
  lynx_view_->LoadTemplate(std::move(meta));
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

void LynxViewImpl::UpdateData(std::shared_ptr<LynxUpdateMeta> meta) {
  if (!meta) {
    return;
  }
  lynx_view_->UpdateData(meta->BuildCore());
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

std::string LynxViewImpl::DumpUITreeForCDP() {
  return lynx_view_ ? lynx_view_->DumpUITreeForCDP() : "";
}

void* LynxViewImpl::GetNativeWindow() {
  return lynx_view_->GetNativeWindow();
}

void LynxViewImpl::Focus() {
#if BUILDFLAG(IS_WIN)
  ::SetFocus(reinterpret_cast<HWND>(lynx_view_->GetNativeWindow()));
#endif
}

void LynxViewImpl::Close() {
  if (self_shared_ptr_) {
    lynx_view_->RemoveClient(self_shared_ptr_);
    self_shared_ptr_.reset();
  }
  // lynx_view_.reset();
}

void LynxViewImpl::EnterForeground() {
  lynx_view_->OnEnterForeground();
}

void LynxViewImpl::EnterBackground() {
  lynx_view_->OnEnterBackground();
}

void LynxViewImpl::OnPageStart(const char* url) {
  std::string url_str = url ? url : "";
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<lynxtron::LynxViewClient> client, std::string url) {
            if (client) {
              client->OnPageStart(url);
            }
          },
          lynx_view_client_, std::move(url_str)));
}

void LynxViewImpl::OnLoadSuccess() {
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<lynxtron::LynxViewClient> client) {
                       if (client) {
                         client->OnLoadSuccess();
                       }
                     },
                     lynx_view_client_));
}

void LynxViewImpl::OnFirstScreen() {
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<lynxtron::LynxViewClient> client) {
                       if (client) {
                         client->OnFirstScreen();
                       }
                     },
                     lynx_view_client_));
}

void LynxViewImpl::OnPageUpdated() {
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<lynxtron::LynxViewClient> client) {
                       if (client) {
                         client->OnPageUpdated();
                       }
                     },
                     lynx_view_client_));
}

void LynxViewImpl::OnDataUpdated() {
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<lynxtron::LynxViewClient> client) {
                       if (client) {
                         client->OnDataUpdated();
                       }
                     },
                     lynx_view_client_));
}

void LynxViewImpl::OnDestroy() {
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<lynxtron::LynxViewClient> client) {
                       if (client) {
                         client->OnDestroy();
                       }
                     },
                     lynx_view_client_));
}

void LynxViewImpl::OnRuntimeReady() {
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<lynxtron::LynxViewClient> client) {
                       if (client) {
                         client->OnRuntimeReady();
                       }
                     },
                     lynx_view_client_));
}

void LynxViewImpl::OnReceivedError(int error_code, const char* message) {
  std::string msg_str = message ? message : "";
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<lynxtron::LynxViewClient> client,
                        int error_code, std::string message) {
                       if (client) {
                         client->OnReceivedError(error_code, message);
                       }
                     },
                     lynx_view_client_, error_code, std::move(msg_str)));
}

void LynxViewImpl::OnTimingSetup(const char* timing_info) {
  std::string info_str = timing_info ? timing_info : "";
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<lynxtron::LynxViewClient> client,
                        std::string timing_info) {
                       if (client) {
                         client->OnTimingSetup(timing_info);
                       }
                     },
                     lynx_view_client_, std::move(info_str)));
}

void LynxViewImpl::OnTimingUpdate(const char* timing_info,
                                  const char* update_timing,
                                  const char* update_flag) {
  std::string info_str = timing_info ? timing_info : "";
  std::string timing_str = update_timing ? update_timing : "";
  std::string flag_str = update_flag ? update_flag : "";
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<lynxtron::LynxViewClient> client,
             std::string timing_info, std::string update_timing,
             std::string update_flag) {
            if (client) {
              client->OnTimingUpdate(timing_info, update_timing, update_flag);
            }
          },
          lynx_view_client_, std::move(info_str), std::move(timing_str),
          std::move(flag_str)));
}

void LynxViewImpl::OnEnterForeground() {
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<lynxtron::LynxViewClient> client) {
                       if (client) {
                         client->OnEnterForeground();
                       }
                     },
                     lynx_view_client_));
}

void LynxViewImpl::OnEnterBackground() {
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<lynxtron::LynxViewClient> client) {
                       if (client) {
                         client->OnEnterBackground();
                       }
                     },
                     lynx_view_client_));
}

void LynxViewImpl::OnFrameTiming(int64_t frame_start_time_in_ns,
                                 int64_t frame_finish_time_in_ns) {
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<lynxtron::LynxViewClient> client, int64_t start,
             int64_t finish) {
            if (client) {
              client->OnFrameTiming(start, finish);
            }
          },
          lynx_view_client_, frame_start_time_in_ns, frame_finish_time_in_ns));
}

}  // namespace lynxtron
