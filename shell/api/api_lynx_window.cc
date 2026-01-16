// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/api/api_lynx_window.h"

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "gin/converter.h"
#include "shell/api/api_app.h"
#include "shell/app/application.h"
#include "shell/app/window_list.h"
#include "shell/common/gin_converters/value_converter.h"
// TODO(Guo Xi): Lynx initialize
#include "shell/common/asar/archive.h"
#include "shell/common/asar/asar_util.h"
#include "shell/common/gin_helper/constructor.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/global_thread.h"
#include "shell/common/node_includes.h"
#include "shell/common/options_switches.h"
#include "shell/common/thread_restrictions.h"
#include "shell/lynx_view_holder/lynx_view.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <TlHelp32.h>
#include <dwmapi.h>

#include <ctime>
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

// bool Write
// void split(const std::string& ,
//            std::vector<std::string>& tokens,
//            const std::string& delimiters = " ") {
//   std::string::size_type lastPos = s.find_first_not_of(delimiters, 0);
//   std::string::size_type pos = s.find_first_of(delimiters, lastPos);
//   while (std::string::npos != pos || std::string::npos != lastPos) {
//     tokens.emplace_back(s.substr(lastPos, pos - lastPos));
//     lastPos = s.find_first_not_of(delimiters, pos);
//     pos = s.find_first_of(delimiters, lastPos);
//   }
// }

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
#endif

}  // namespace

namespace api {

// const std::string kLynxError = "--lynx-error";
// const std::string kLynxContainerError = "--lynx-container-error";
const std::string kEventOnLoadOnlineTemplate = "on-load-online-template";
// const std::string kEventOnLoadLocalTemplate = "on-load-local-template";

enum class ErrorCode : int32_t { kOK = 0, kFatalError = 100 };
std::map<std::string, std::string> GetQueryKeyValueMap(
    const std::string& spec) {
  url::Parsed parsed;
  ParseStandardURL(spec.c_str(), spec.length(), &parsed);
  url::Component query = parsed.query;

  std::map<std::string, std::string> map;
  url::Component key, value;
  while (ExtractQueryKeyValue(spec.c_str(), &query, &key, &value)) {
    std::string s_key = spec.substr(key.begin, key.len);
    std::string s_value = spec.substr(value.begin, value.len);

    map[s_key] = std::move(s_value);
    key.reset();
    value.reset();
  }
  return map;
}

LynxWindow::LynxWindow(gin::Arguments* args,
                       const gin_helper::Dictionary& options)
    : BaseWindow(args->isolate(), options) {
  // lynx_view_holder_group_->AddLynxViewHolderClient(this);
  // lynx_view_->SetClient(this);
  // #if BUILDFLAG(IS_WIN)
  //   lynx_bridge_ =
  //   std::make_shared<LynxNativeModule>(weak_factory_.GetWeakPtr());
  //   hybrid_monitor_ =
  //       std::make_shared<HybridMonitorModule>(weak_factory_.GetWeakPtr());
  //   js_resource_provider_ =
  //       std::make_unique<ResourceProviderImpl>("EXTERNAL_JS_SOURCE");
  //   dynamic_component_provider_ =
  //       std::make_unique<ResourceProviderImpl>("DYNAMIC_COMPONENT");
  // #endif
  InitWithArgs(args);
  options.Get("software_render", &software_render_);
  // software_render_ = false;
  //  Init window after everything has been setup.
  window()->InitFromOptions(options);

  // node integration
  if (options.ValueOrDefault(options::kNodeIntegration, false)) {
    // std::cout << "node integration is enabled" << std::endl;
    node_integration_ = true;
  } else {
    // std::cout << "node integration is disabled" << std::endl;
    node_integration_ = false;
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
  // util::LynxGlobalInit();
  // #if defined(OS_MAC)
  //   LynxNapiBridgeModule::RegisterMoudle();
  // #endif
}

LynxWindow::~LynxWindow() {
  // #if BUILDFLAG(IS_NODE_LYNX) || BUILDFLAG(IS_WIN)
  //   lynx_view_holder_group_->RemoveLynxViewHolderGroupClient(this);
  // #endif
}

void LynxWindow::RequestPreferredWidth(int* width) {}

void LynxWindow::OnCloseButtonClicked(bool& prevent_default) {
  // When user tries to close the window by clicking the close button, we do
  // not close the window immediately, instead we try to close the web page
  // first, and when the web page is closed the window will also be closed.
  prevent_default = true;

  // Assume the window is not responding if it doesn't cancel the close and is
  // not closed in 5s, in this way we can quickly show the unresponsive
  // dialog when the window is busy executing some script without waiting for
  // the unresponsive timeout.
  // if (window_unresponsive_closure_.IsCancelled()) {
  //   ScheduleUnresponsiveEvent(5000);
  // }
  GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<LynxWindow> window) {
                       if (window) {
                         window->CloseImmediately();
                       }
                     },
                     GetWeakPtr()));

}  // namespace api

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
  // lynx::LynxRect rect;
  // RECT win_rect{};
  // ::GetClientRect(window_->GetNativeWindowHandle(), &win_rect);
  // auto dpi = display::win::ScreenWin::GetScaleFactorForHWND(
  //     window_->GetNativeWindowHandle());
  // rect = {0.f, 0.f, static_cast<float>((win_rect.right - win_rect.left) /
  // dpi),
  //         static_cast<float>((win_rect.bottom - win_rect.top) / dpi)};
  // MoveWindow(CurrentLynxViewHolder()->GetHwnd(), win_rect.left, win_rect.top,
  //            win_rect.right - win_rect.left, win_rect.bottom - win_rect.top,
  //            TRUE);
#endif

  // Update LynxView layout and screen parameters
  if (lynx_view_) {
    lynx_view_->SetBounds(window_->GetBounds());  // Set view bounds
    const float width = window_->GetSize().width();
    const float height = window_->GetSize().height();
    lynx_view_->UpdateScreenMetrics(
        width, height,
        window_->GetDevicePixelRatio());  // Update screen size and device pixel
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

// TODO(Guo Xi): implement with napi
// void LynxWindow::Send(const std::string& channel, gin::Arguments* arguments)
// {
//   lynx::EncodableList encodable_args;
//   arguments->GetRemaining(&encodable_args);
//   auto props_str = gin::EncodableValueToJsonStr(
//       isolate(), lynx::EncodableValue{encodable_args});
//   if (CurrentLynxViewHolder()) {
//     CurrentLynxViewHolder()->SendGlobalEvent(channel, props_str);
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
  lynx_view_ = LynxView::Create();
  auto source = LoadFileData(local_url);
  lynx_view_->Init(GetBounds().width(), GetBounds().height(), 1.0,
                   window_->GetNativeWindowHandle(), node_integration_);
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

// bool LynxWindow::CheckLynxViewExit(const lynx::LynxView* lynx_view) {
//   if (!lynx_view_holder_group_) {
//     return false;
//   }
//   return lynx_view_holder_group_->Exist(lynx_view);
// }

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

bool LynxWindow::UpdateDataWithString(const std::string& data) {
  // if (lynx_view_) {
  //   lynx_view_->UpdateDataWithString(data);
  // }

  return true;
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

// bool LynxWindow::RequestLayoutWhenSafepointEnable() {
// #if BUILDFLAG(IS_WIN)
//   if (CurrentLynxViewHolder()) {
//     CurrentLynxViewHolder()->RequestLayoutWhenSafepointEnable();
//   }
// #endif
//   return true;
// }

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

  lynx_view_->UpdateData(data_string.value(), global_props_string.value());
  return true;
}

// bool LynxWindow::SetGlobalProp(v8::Isolate* isolate,
//                                v8::Local<v8::Value> value) {
//   gin::ConvertFromV8(isolate, value, &global_props_);
//   return true;
// }

void LynxWindow::OnPageStart(std::string_view url) {
  // if (lynx_monitor_) {
  //   lynx_monitor_->OnPageStart(lynx_view_holder, url);
  // }
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
void LynxWindow::OnRuntimeReady() {
  // if (lynx_monitor_) {
  //   lynx_monitor_->OnRuntimeReady(lynx_view_holder);
  // }
}

void LynxWindow::OnReceivedError(int error_code, std::string_view message) {
  // if (lynx_monitor_) {
  //   lynx_monitor_->OnReceivedError(lynx_view_holder, error_code, message);
  // }

  // ReportErrorToNode(kLynxError, error_code, message);
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

// void LynxWindow::OnFirstLoadPerfReady(
//     LynxView* lynx_view,
//     const std::unordered_map<int32_t, double>& perf,
//     const std::unordered_map<int32_t, std::string>& perf_timing) {
//   // if (lynx_monitor_) {
//   //   lynx_monitor_->OnFirstLoadPerfReady(lynx_view_holder, perf,
//   perf_timing);
//   // }
// }

// base::FilePath WriteTemplateFile(const std::vector<char>& data) {
//   base::FilePath path;
//   if (!base::CreateTemporaryFile(&path)) {
//     return {};
//   }

//   if (base::WriteFile(path, data.data(), data.size()) == -1) {
//     return {};
//   }
//   return path;
// }

// bool LynxWindow::SendGlobalEventSafely(const lynx::LynxView* lynx_view,
//                                        const std::string& name,
//                                        const std::string& json_params) {
//   auto* lynx_view_inter = const_cast<lynx::LynxView*>(lynx_view);
//   if (!lynx_view_inter) {
//     return false;
//   }
//   if (!CheckLynxViewExit(lynx_view_inter)) {
//     return false;
//   }
//   if (lynx_view_holder_group_) {
//     lynx_view_holder_group_->GetLynxViewHolder(lynx_view_inter)
//         ->SendGlobalEvent(name, json_params);
//   }
//   return true;
// }

// bool LynxWindow::CloseByLynxBridge(const lynx::LynxView* lynx_view,
//                                    const std::string& animation_type,
//                                    const lynx::EncodableList& container_list)
//                                    {
//   Close();
//   return true;
// }

// lynx::LynxViewHolder* LynxWindow::CurrentLynxViewHolder() {
//   if (lynx_view_holder_group_) {
//     return lynx_view_holder_group_->GetCurrentLynxViewHolder();
//   }
//   return nullptr;
// }

void LynxWindow::OnDataUpdated() {
  // Emit("on-data-updated");
}

void LynxWindow::OnPageUpdated() {
  // Emit("on-page-updated");
}

void LynxWindow::UpdateWindowLynxViewPos() {
#if BUILDFLAG(IS_WIN)
  // TODO(Guo Xi): implement it
  // if (CurrentLynxViewHolder()) {
  //   RECT rect{};
  //   ::GetClientRect(window_->GetNativeWindowHandle(), &rect);
  //   LONG xy = MAKELONG(rect.right - rect.left, rect.bottom - rect.top);
  //   ::SendMessage(CurrentLynxViewHolder()->GetHwnd(), WM_SIZE,
  //                 WPARAM(SIZE_MAXSHOW), (LPARAM)xy);
  // }
#endif
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
// bool LynxWindow::ReportJSError(const LynxView* lynx_view,
//                                const std::string& error_info) {
//   if (!lynx_monitor_ || !lynx_view_holder_group_) {
//     return false;
//   }
//   auto* lynx_view_holder =
//       lynx_view_holder_group_->GetLynxViewHolder(lynx_view);
//   lynx_monitor_->ReportJSError(lynx_view_holder, error_info);
//   return true;
// }

// bool LynxWindow::ConfigJSBase(const lynx::LynxView* lynx_view,
//                               const std::string& bid) {
//   if (!lynx_monitor_ || !lynx_view_holder_group_) {
//     return false;
//   }
//   auto* lynx_view_holder =
//       lynx_view_holder_group_->GetLynxViewHolder(lynx_view);
//   lynx_monitor_->ConfigJSBase(lynx_view_holder, bid);
//   return true;
// }

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
      .SetMethod("updateData", &LynxWindow::UpdateData)
      .SetMethod("sendGlobalEvent", &LynxWindow::SendGlobalEvent);

  // .SetMethod("Send", &LynxWindow::Send)
  // .SetMethod("OpenScheme", &LynxWindow::OpenScheme);
  // .SetMethod("SetGlobalProp", &LynxWindow::SetGlobalProp);
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
// bool LynxWindow::CustomReport(const lynx::LynxView* lynx_view,
//                               const std::string& custom_data) {
//   if (!lynx_monitor_ || !lynx_view_holder_group_) {
//     return false;
//   }
//   auto* lynx_view_holder =
//       lynx_view_holder_group_->GetLynxViewHolder(lynx_view);
//   lynx_monitor_->ReportCustomData(lynx_view_holder, custom_data);
//   return true;
// }

// void LynxWindow::LynxVerify(const std::vector<uint8_t>& source,
//                             const std::string& scheme,
//                             int lynx_verify_mode,
//                             const std::string& public_key,
//                             base::RepeatingCallback<void(bool)> callback) {
//   auto event = gin_helper::internal::CreateLynxEvent(
//       isolate(), GetWrapper(),
//       base::BindRepeating(
//           [](v8::Isolate* isolate, v8::Local<v8::Value> result) {
//             LOG(INFO) << "lynx file verify complete";
//           }));
//   bool result = true;
//   if (lynx_verify_mode != 0) {
//     result = util::VerifyLynxSignature(source, public_key);
//   }
//   v8::HandleScope handle_scope(isolate());
//   auto object = gin_helper::Dictionary::CreateEmpty(isolate());
//   object.Set("verifyResult", result ? 1 : 0);
//   object.Set("scheme", scheme);
//   bool prevent_default =
//       EmitLynxEvent("lynx-file-verify-complete", event, object);
//   std::move(callback).Run(!prevent_default);
// }

// void LynxWindow::CheckLynxValidation(
//     const std::vector<uint8_t>& source,
//     const std::string& channel_name,
//     const std::string& scheme,
//     base::RepeatingCallback<void(bool)> callback) {
//   if (!Lynx::Get()->IsLynxVerifyEnable()) {
//     std::move(callback).Run(true);
//     return;
//   }

//   auto event_callback = [](base::WeakPtr<LynxWindow> window,
//                            const std::string& scheme,
//                            base::RepeatingCallback<void(bool)> callback,
//                            const std::vector<uint8_t>& source,
//                            v8::Isolate* isolate, v8::Local<v8::Value> result)
//                            {
//     gin::Dictionary dict(isolate);
//     gin::ConvertFromV8(isolate, result, &dict);
//     int lynx_verify_mode = 1;  // 0: not verify, 1: default verify
//     std::string public_key;
//     if (!dict.Get("lynxVerifyMode", &lynx_verify_mode)) {
//       LOG(ERROR) << "[LynxWindow] can't find lynxVerifyMode";
//     }
//     if (!dict.Get("publicKey", &public_key)) {
//       LOG(ERROR) << "[LynxWindow] can't find publicKey";
//     }
//     window->LynxVerify(source, scheme, lynx_verify_mode, public_key,
//                        std::move(callback));
//   };
//   auto feId = util::GetLynxFeID(source);
//   LOG(ERROR) << "LynxWindow::GetLynxFeID" << feId;
//   v8::HandleScope handle_scope(isolate());
//   auto object = gin_helper::Dictionary::CreateEmpty(isolate());
//   object.Set("channel", channel_name);
//   object.Set("feId", feId);

//   auto event = gin_helper::internal::CreateLynxEvent(
//       isolate(), GetWrapper(),
//       base::BindRepeating(event_callback, weak_factory_.GetWeakPtr(), scheme,
//                           std::move(callback), source));

//   LOG(ERROR) << "LynxWindow::CheckLynxValidation";

//   EmitLynxEvent("lynx-file-load", event, object);
// }

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

NODE_LINKED_BINDING_CONTEXT_AWARE(electron_lynx_window, Initialize)
