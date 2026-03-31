// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_lynx_window.h"

#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "gin/converter.h"
#include "lynx/platform/embedder/public/capi/lynx_env_capi.h"
#include "shell/api/api_app.h"
#include "shell/api/lynx_view/lynx_view.h"
#include "shell/api/lynx_view_monitor_delegate.h"
#include "shell/api/lynx_window_manager.h"
#include "shell/app/application.h"
#include "shell/app/window_list.h"
#include "shell/common/asar/archive.h"
#include "shell/common/asar/asar_util.h"
#include "shell/common/gin_converters/value_converter.h"
#include "shell/common/gin_helper/constructor.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/global_thread.h"
#include "shell/common/node_includes.h"
#include "shell/common/options_switches.h"
#include "shell/common/thread_restrictions.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <dwmapi.h>
#endif

namespace lynxtron {
namespace {

std::optional<std::string> ConvertDictionaryToJsonString(
    const gin_helper::Dictionary& json) {
  base::Value::Dict dict;
  if (!gin::ConvertFromV8(json.isolate(), json.GetHandle(), &dict)) {
    return std::nullopt;
  }
  std::string json_string;
  if (!base::JSONWriter::Write(dict, &json_string)) {
    return std::nullopt;
  }
  return json_string;
}

std::vector<uint8_t> LoadFileData(std::string_view path) {
  base::FilePath in_file_name = base::FilePath::FromUTF8Unsafe(path);
  std::string file_contents;
  {
    ScopedAllowBlockingForLynxtron allow_blocking;
    if (!asar::ReadFileToString(in_file_name, &file_contents)) {
      return {};
    }
  }
  size_t size = file_contents.size();
  std::vector<uint8_t> buf(file_contents.data(), file_contents.data() + size);
  return buf;
}

#if BUILDFLAG(IS_WIN)
void UpdateFramebufferTransparency(HWND window) {
  BOOL enabled;
  if (SUCCEEDED(::DwmIsCompositionEnabled(&enabled)) && enabled) {
    HRGN region = ::CreateRectRgn(0, 0, -1, -1);
    DWM_BLURBEHIND bb = {0};
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.hRgnBlur = region;
    bb.fEnable = TRUE;

    if (SUCCEEDED(::DwmEnableBlurBehindWindow(window, &bb))) {
      // Decorated windows don't repaint the transparent background
      // leaving a trail behind animations
      // HACK: Making the window layered with a transparency color key
      //       seems to fix this.  Normally, when specifying
      //       a transparency color key to be used when composing the
      //       layered window, all pixels painted by the window in this
      //       color will be transparent.  That doesn't seem to be the
      //       case anymore, at least when used with blur behind window
      //       plus negative region.
      LONG exStyle = ::GetWindowLongW(window, GWL_EXSTYLE);
      exStyle |= WS_EX_LAYERED;
      ::SetWindowLongW(window, GWL_EXSTYLE, exStyle);
      ::SetLayeredWindowAttributes(window, 0x0, 255, LWA_ALPHA);
    } else {
      LONG exStyle = ::GetWindowLongW(window, GWL_EXSTYLE);
      exStyle &= ~WS_EX_LAYERED;
      ::SetWindowLongW(window, GWL_EXSTYLE, exStyle);
      ::RedrawWindow(window, NULL, NULL,
                     RDW_ERASE | RDW_INVALIDATE | RDW_FRAME);
    }
    ::DeleteObject(region);
  }
}
#endif /*  */

}  // namespace

namespace api {
const std::string kLynxError = "--lynx-error";
enum class ErrorCode : int32_t { kOK = 0, kFatalError = 100 };

LynxWindow::LynxWindow(gin::Arguments* args,
                       const gin_helper::Dictionary& options)
    : BaseWindow(args->isolate(), options) {
  InitWithArgs(args);
  // TODO(Guo Xi): support software render.
  options.Get("software_render", &software_render_);

  window()->InitFromOptions(options);

  gin_helper::Dictionary node_integration_config;
  if (options.Get(options::kNodeIntegration, &node_integration_config)) {
    node_integration_config.Get("preload_paths", &node_integration_preload_);
  }

  // transparent
  bool transparent = false;
  options.Get(options::kTransparent, &transparent);
#if BUILDFLAG(IS_WIN)
  if (transparent) {
    UpdateFramebufferTransparency(window_->GetNativeWindowHandle());
    ::SetLayeredWindowAttributes(window_->GetNativeWindowHandle(), RGB(0, 0, 0),
                                 255, LWA_COLORKEY);
  }
#endif
  // init lynx env.
  lynx_env_set_devtool_app_info("sdkVersion", lynx_env_get_sdk_version());
  auto* application = lynxtron::Application::Get();
  lynx_env_set_devtool_app_info("appId", application->GetAppId().c_str());
  lynx_env_set_devtool_app_info("App", application->GetName().c_str());
  auto& registry = GetGlobalDelegateRegistry();
  auto it = registry.find(kLynxViewMonitorDelegateName);
  if (it != registry.end()) {
    auto delegate = it->second.CreateDelegate();
    if (delegate) {
      lynx_view_monitor_delegate_ = std::unique_ptr<LynxViewMonitorDelegate>(
          reinterpret_cast<LynxViewMonitorDelegate*>(delegate.release()));
    }
  } else {
    LOG(ERROR) << "LynxViewMonitorDelegate not found in registry.";
  }
  LynxWindowManager::GetInstance()->RegisterLynxWindow(GetWeakPtr());
}

LynxWindow::~LynxWindow() {
  LynxWindowManager::GetInstance()->UnregisterLynxWindow(GetWeakPtr());
}

// void LynxWindow::OnCloseRequested(bool& prevent_default) {
//   // When user tries to close the window by clicking the close button, we do
//   // not close the window immediately, instead we try to close the web page
//   // first, and when the web page is closed the window will also be closed.
//   prevent_default = true;

//   // Assume the window is not responding if it doesn't cancel the close and
//   is
//   // not closed in 5s, in this way we can quickly show the unresponsive
//   // dialog when the window is busy executing some script without waiting for
//   // the unresponsive timeout.
//   // if (window_unresponsive_closure_.IsCancelled()) {
//   //   ScheduleUnresponsiveEvent(5000);
//   // }
//   GlobalThread::GetUIThreadTaskRunner()->PostTask(
//       FROM_HERE, base::BindOnce(
//                      [](base::WeakPtr<LynxWindow> window) {
//                        if (window) {
//                          window->CloseImmediately();
//                        }
//                      },
//                      GetWeakPtr()));

// }  // namespace api

void LynxWindow::OnWindowBlur() {
  BaseWindow::OnWindowBlur();
}

void LynxWindow::OnWindowFocus() {
  FocusLynxView();
  BaseWindow::OnWindowFocus();
}

void LynxWindow::OnWindowIsKeyChanged(bool is_key) {}

void LynxWindow::OnWindowResize() {
  if (IsMinimized()) {
    return;
  }

#if BUILDFLAG(IS_WIN)
  // Update LynxView native window size to match client area
  RECT win_rect{};
  ::GetClientRect(window_->GetNativeWindowHandle(), &win_rect);
  if (lynx_view_) {
    HWND lynx_hwnd = reinterpret_cast<HWND>(lynx_view_->GetNativeWindow());
    if (lynx_hwnd) {
      MoveWindow(lynx_hwnd, win_rect.left, win_rect.top,
                 win_rect.right - win_rect.left, win_rect.bottom - win_rect.top,
                 TRUE);
    }
  }
#endif

  // Update LynxView layout and screen parameters
  if (lynx_view_) {
    float width = window_->GetSize().width();
    float height = window_->GetSize().height();
    const float device_pixel_ratio = window_->GetDevicePixelRatio();
#if BUILDFLAG(IS_WIN)
    // Use client area size instead of window size to avoid being covered by
    // window borders
    RECT win_rect{};
    ::GetClientRect(window_->GetNativeWindowHandle(), &win_rect);
    width =
        static_cast<float>(win_rect.right - win_rect.left) / device_pixel_ratio;
    height =
        static_cast<float>(win_rect.bottom - win_rect.top) / device_pixel_ratio;
#endif

    lynx_view_->UpdateScreenMetrics(
        width, height,
        device_pixel_ratio);  // Update screen size and device pixel
                              // ratio
    lynx_view_->SetFrame(
        0, 0, width, height);  // Set view position and size in parent container
  }
  BaseWindow::OnWindowResize();
}

void LynxWindow::OnWindowResized() {
  FocusLynxView();
  BaseWindow::OnWindowResized();
}

void LynxWindow::OnWindowRestore() {
  // #if BUILDFLAG(IS_WIN)
  //   if (CurrentLynxViewHolder()) {
  //     CurrentLynxViewHolder()->SetFocus();
  //     CurrentLynxViewHolder()->OnEnterForeground();
  //   }
  // #endif

  BaseWindow::OnWindowRestore();
}

void LynxWindow::OnWindowMinimize() {
  // #if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_NODE_LYNX)
  //   if (CurrentLynxViewHolder()) {
  //     CurrentLynxViewHolder()->OnEnterBackground();
  //   }
  // #endif
  BaseWindow::OnWindowMinimize();
}

void LynxWindow::OnWindowMove() {
  BaseWindow::OnWindowMove();
}

void LynxWindow::OnWindowWillMove(const gfx::Rect& new_bounds,
                                  bool& prevent_default) {
  BaseWindow::OnWindowWillMove(new_bounds, prevent_default);
}

void LynxWindow::OnWindowLeaveFullScreen() {
  BaseWindow::OnWindowLeaveFullScreen();
}

void LynxWindow::OnWindowClosed() {
  BaseWindow::OnWindowClosed();
}

#if BUILDFLAG(IS_WIN)
void LynxWindow::OnWindowMessage(UINT message, WPARAM w_param, LPARAM l_param) {
  switch (message) {
    case WM_DPICHANGED: {
      // TODO(Guo Xi): handle dpi change
      // if (!lynx_view_) {
      //   break;
      // }

      // if (lynx_view_->GetNativeWindow() != nullptr) {
      //   // #define WM_DPICHANGED_BEFOREPARENT       0x02E2
      //   SendMessage(lynx_view_->GetNativeWindow(), 0x02E2, 0, 0);
      // }
    } break;
    default:
      break;
  }
  BaseWindow::OnWindowMessage(message, w_param, l_param);
}
#endif

void LynxWindow::CloseImmediately() {
  // Close all child windows before closing current window.
  v8::Locker locker(isolate());
  v8::HandleScope handle_scope(isolate());
  for (v8::Local<v8::Value> value : child_windows_.Values(isolate())) {
    gin_helper::Handle<LynxWindow> child;
    if (gin::ConvertFromV8(isolate(), value, &child) && !child.IsEmpty()) {
      child->window()->CloseImmediately();
    }
  }
  lynx_view_ = nullptr;
  // Close all lynx view before closing current window.
  // if (lynx_view_holder_group_) {
  //   lynx_view_holder_group_->Clear();
  // }

  BaseWindow::CloseImmediately();

  // Do not sent "unresponsive" event after window is closed.
  // window_unresponsive_closure_.Cancel();
}

void LynxWindow::Focus() {
  BaseWindow::Focus();
}

void LynxWindow::Blur() {
  BaseWindow::Blur();
}

// TODO(Guo Xi): whether support background color
void LynxWindow::SetBackgroundColor(const std::string& color_name) {
  BaseWindow::SetBackgroundColor(color_name);
}

// void LynxWindow::ScheduleUnresponsiveEvent(int ms) {
//   if (!window_unresponsive_closure_.IsCancelled())
//     return;

//   window_unresponsive_closure_.Reset(
//       base::BindRepeating(&LynxWindow::NotifyWindowUnresponsive,
//       GetWeakPtr()));
//   base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
//       FROM_HERE, window_unresponsive_closure_.callback(),
//       base::Milliseconds<int>(ms));
// }

// void LynxWindow::NotifyWindowUnresponsive() {
//   window_unresponsive_closure_.Cancel();
//   if (!window_->IsClosed() && window_->IsEnabled()) {
//     Emit("unresponsive");
//   }
// }

void LynxWindow::CreateLynxView(const std::string& local_url,
                                const std::string& global_props,
                                const std::string& initial_props,
                                const std::string& group_name,
                                const std::string& channel_name,
                                const std::string& scheme) {
  if (lynx_view_) {
    return;
  }
  lynx_view_ = LynxView::Create(GetWeakPtr());
  auto source = LoadFileData(local_url);
  float width = window_->GetSize().width();
  float height = window_->GetSize().height();
  float device_pixel_ratio = window_->GetDevicePixelRatio();
#if BUILDFLAG(IS_WIN)
  RECT win_rect{};
  ::GetClientRect(window_->GetNativeWindowHandle(), &win_rect);
  width =
      static_cast<float>(win_rect.right - win_rect.left) / device_pixel_ratio;
  height =
      static_cast<float>(win_rect.bottom - win_rect.top) / device_pixel_ratio;
#endif

  lynx_view_->Init(width, height, device_pixel_ratio,
                   window_->GetNativeWindowHandle(), node_integration_preload_);
  if (data_str_.has_value() && global_props_.has_value()) {
    lynx_view_->UpdateData(data_str_.value(), global_props_.value());
    data_str_.reset();
    global_props_.reset();
  }
  lynx_view_->LoadTemplate(local_url, source);
  lynx_view_->SetClient(weak_factory_.GetWeakPtr());
}

void LynxWindow::CloseLynxView() {
  if (!lynx_view_) {
    return;
  }
  lynx_view_->Close();
}

void LynxWindow::FocusLynxView() {
  // if (CurrentLynxViewHolder() && CurrentLynxViewHolder()->IsShow()) {
  //   CurrentLynxViewHolder()->SetFocus();
  // }
}

void LynxWindow::SetFpsMonitorEnabled(bool enabled,
                                      uint32_t sample_interval_millis) {
  bool state_changed = enable_fps_monitor_ != enabled;
  enable_fps_monitor_ = enabled;
  sample_interval_millis_ = sample_interval_millis;
  if (state_changed) {
    StartFpsMonitorTask();
  }
}

void LynxWindow::StartFpsMonitorTask() {
  GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<LynxWindow> window) {
            if (!window) {
              return;
            }
            window->EmitFpsEvent();
            if (window->enable_fps_monitor_) {
              window->StartFpsMonitorTask();
            }
          },
          GetWeakPtr()),
      base::Milliseconds(sample_interval_millis_));
}

void LynxWindow::EmitFpsEvent() {
  if (last_frame_timings_.empty()) {
    return;
  }
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Value> arg = gin::ConvertToV8(isolate(), last_frame_timings_);

  last_frame_timings_.clear();
  Emit("frame-timings", arg);
}

void LynxWindow::OnWindowShow() {
  if (lynx_view_) {
    lynx_view_->Show();
  }
  BaseWindow::OnWindowShow();
}

void LynxWindow::OnWindowHide() {
  if (lynx_view_) {
    lynx_view_->Hide();
  }
  BaseWindow::OnWindowHide();
}

bool LynxWindow::LoadFile(const std::string& path, gin::Arguments* args) {
  base::FilePath local_path = base::FilePath::FromUTF8Unsafe(path.data());
  base::FilePath asar_path, relative_path;
  if (!base::PathExists(local_path)) {
    // if file exists in asar
    if (!asar::GetAsarArchivePath(local_path, &asar_path, &relative_path)) {
      LOG(ERROR) << "File not found path: " << path;
      return false;
    }
    std::shared_ptr<asar::Archive> archive =
        asar::GetOrCreateAsarArchive(asar_path);
    asar::Archive::FileInfo info;
    if (!archive || !archive->GetFileInfo(relative_path, &info)) {
      LOG(ERROR) << "File not found in asar: " << path;
      return false;
    }
  }

  CreateLynxView(local_path.AsUTF8Unsafe(), local_path.AsUTF8Unsafe(), "", "",
                 "", "");
  return true;
}

bool LynxWindow::LoadUrl(const std::string& url) {
  CreateLynxView(url, "", "", "", "", "");
  return true;
}

bool LynxWindow::ReloadTemplate(const gin_helper::Dictionary& data,
                                const gin_helper::Dictionary& global_props) {
  if (!lynx_view_) {
    return false;
  }

  auto data_string = ConvertDictionaryToJsonString(data);
  if (!data_string.has_value()) {
    return false;
  }

  auto global_props_string = ConvertDictionaryToJsonString(global_props);
  if (!global_props_string.has_value()) {
    return false;
  }

  lynx_view_->ReloadTemplate(data_string.value(), global_props_string.value());
  return true;
}

bool LynxWindow::UpdateData(const gin_helper::Dictionary& data,
                            const gin_helper::Dictionary& global_props) {
  auto data_string = ConvertDictionaryToJsonString(data);
  if (!data_string.has_value()) {
    return false;
  }

  auto global_props_string = ConvertDictionaryToJsonString(global_props);
  if (!global_props_string.has_value()) {
    return false;
  }

  if (!lynx_view_) {
    data_str_ = std::move(data_string);
    global_props_ = std::move(global_props_string);
  } else {
    lynx_view_->UpdateData(data_string.value(), global_props_string.value());
  }
  return true;
}

void LynxWindow::OnPageStart(std::string_view url) {
  // Emit("on-page-start", url);
  if (lynx_view_monitor_delegate_) {
    lynx_view_monitor_delegate_->SetInstanceId(
        reinterpret_cast<int64_t>(lynx_view_.get()));
    lynx_view_monitor_delegate_->OnPageStart(url);
  }
}

/**
 * page load success
 */
void LynxWindow::OnLoadSuccess() {
  Emit("on-load-success");
}

/**
 * first screen layout complete
 */
void LynxWindow::OnFirstScreen() {
  Emit("on-first-screen");
}

void LynxWindow::OnDestroy() {
  // auto* event_center = EventCenter::GetDefault();
  // event_center->UnregisterSubscriber(std::make_unique<LynxSubscriber>(
  //     0, static_cast<lynx::LynxView*>(lynx_view_holder->GetLynxView()),
  //     GetWeakPtr()));
  // if (lynx_monitor_) {
  //   lynx_monitor_->OnDestroy(lynx_view_holder);
  // }
}

/**
 * notify JS Runtime initialization complete
 */
void LynxWindow::OnRuntimeReady() {}

void LynxWindow::OnReceivedError(int error_code, std::string_view message) {
  if (lynx_view_monitor_delegate_) {
    lynx_view_monitor_delegate_->OnReceivedError(error_code, message);
  }
  ReportErrorToNode(kLynxError, error_code, std::string(message));
}

void LynxWindow::OnTimingSetup(std::string_view timing_info) {
  // Emit("on-timing-setup", timing_info);
}

void LynxWindow::OnTimingUpdate(std::string_view timing_info,
                                std::string_view update_timing,
                                std::string_view update_flag) {
  // Emit("on-timing-update", timing_info, update_timing, update_flag);
}

void LynxWindow::OnEnterForeground() {
  // Emit("on-enter-foreground");
}

void LynxWindow::OnEnterBackground() {
  // Emit("on-enter-background");
}

void LynxWindow::OnDataUpdated() {
  // Emit("on-data-updated");
}

void LynxWindow::OnPageUpdated() {
  // Emit("on-page-updated");
}

void LynxWindow::ReportErrorToNode(const std::string& error_type,
                                   const int32_t error_code,
                                   const std::string& message) {
  GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<LynxWindow> window, const std::string& error_type,
             int32_t error_code, const std::string& message) {
            if (window) {
              window->Emit("receive-lynx-window-error", error_type, error_code,
                           message);
            }
          },
          GetWeakPtr(), error_type, error_code, message));
}

bool LynxWindow::SendGlobalEvent(const std::string& name,
                                 const gin_helper::Dictionary& json) {
  if (!lynx_view_) {
    return false;
  }

  auto json_string = ConvertDictionaryToJsonString(json);
  if (!json_string.has_value()) {
    return false;
  }

  lynx_view_->SendGlobalEvent(name, json_string.value());
  return true;
}

// static
gin_helper::WrappableBase* LynxWindow::New(gin_helper::ErrorThrower thrower,
                                           gin::Arguments* args) {
  // if (!Browser::Get()->is_ready()) {
  //   thrower.ThrowError("Cannot create BrowserWindow before app is ready");
  //   return nullptr;
  // }

  if (args->Length() > 1) {
    args->ThrowError();
    return nullptr;
  }

  gin_helper::Dictionary options;
  if (!(args->Length() == 1 && args->GetNext(&options))) {
    options = gin::Dictionary::CreateEmpty(args->isolate());
  }

  return new LynxWindow(args, options);
}

// static
void LynxWindow::BuildPrototype(v8::Isolate* isolate,
                                v8::Local<v8::FunctionTemplate> prototype) {
  prototype->SetClassName(gin::StringToV8(isolate, "LynxWindow"));
  gin_helper::ObjectTemplateBuilder(isolate, prototype->PrototypeTemplate())
      .SetMethod("loadFile", &LynxWindow::LoadFile)
      .SetMethod("loadURL", &LynxWindow::LoadUrl)
      .SetMethod("updateData", &LynxWindow::UpdateData)
      .SetMethod("sendGlobalEvent", &LynxWindow::SendGlobalEvent);
}

// static
v8::Local<v8::Value> LynxWindow::From(v8::Isolate* isolate,
                                      NativeWindow* native_window) {
  auto* existing = TrackableObject::FromWrappedClass(isolate, native_window);
  if (existing) {
    return existing->GetWrapper();
  } else {
    return v8::Null(isolate);
  }
}
}  // namespace api

}  // namespace lynxtron

namespace {

using lynxtron::api::LynxWindow;

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* const isolate = lynxtron::JavascriptEnvironment::GetIsolate();
  gin_helper::Dictionary dict(isolate, exports);
  dict.Set("LynxWindow", gin_helper::CreateConstructor<LynxWindow>(
                             isolate, base::BindRepeating(&LynxWindow::New)));
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_lynx_window, Initialize)
