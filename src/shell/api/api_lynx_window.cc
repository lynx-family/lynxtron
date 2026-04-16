// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_lynx_window.h"

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "gin/converter.h"
#include "lynx/platform/embedder/public/capi/lynx_env_capi.h"
#include "shell/api/api_app.h"
#include "shell/api/api_lynx_template_bundle.h"
#include "shell/api/lynx_view/lynx_update_meta.h"
#include "shell/api/lynx_view/lynx_view.h"
#include "shell/api/lynx_view/lynx_view_builder.h"
#include "shell/api/lynx_view_state_observer.h"
#include "shell/api/lynx_window_manager.h"
#include "shell/app/application.h"
#include "shell/app/window_list.h"
#include "shell/common/asar/archive.h"
#include "shell/common/asar/asar_util.h"
#include "shell/common/gin_helper/constructor.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/global_thread.h"
#include "shell/common/node_includes.h"
#include "shell/common/options_switches.h"
#include "shell/common/thread_restrictions.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <dwmapi.h>
#endif

namespace lynxtron {
namespace {

std::optional<std::string> ConvertDictionaryToJsonString(
    const gin_helper::Dictionary& json) {
  v8::Isolate* isolate = json.isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::String> json_string;
  if (!v8::JSON::Stringify(context, json.GetHandle()).ToLocal(&json_string)) {
    return std::nullopt;
  }
  return gin::V8ToString(isolate, json_string);
}

bool ExtractOptionalDictionary(v8::Isolate* isolate,
                               const gin_helper::Dictionary& options,
                               std::string_view key,
                               gin_helper::Dictionary* out) {
  v8::Local<v8::Value> value;
  if (!options.Get(key, &value) || value->IsUndefined() || value->IsNull()) {
    return false;
  }
  if (!value->IsObject()) {
    return false;
  }
  *out = gin_helper::Dictionary(isolate, value.As<v8::Object>());
  return true;
}

bool ExtractLoadDataOptions(gin::Arguments* args,
                            gin_helper::Dictionary* data,
                            gin_helper::Dictionary* global_props) {
  v8::Isolate* isolate = args->isolate();
  *data = gin::Dictionary::CreateEmpty(isolate);
  *global_props = gin::Dictionary::CreateEmpty(isolate);

  if (args->Length() <= 0) {
    return true;
  }

  v8::Local<v8::Value> options_value;
  if (!args->GetNext(&options_value)) {
    return true;
  }
  if (options_value->IsUndefined() || options_value->IsNull()) {
    return true;
  }
  if (!options_value->IsObject()) {
    args->ThrowTypeError("options must be an object");
    return false;
  }

  gin_helper::Dictionary options(isolate, options_value.As<v8::Object>());
  if (!ExtractOptionalDictionary(isolate, options, "data", data) &&
      options.Has("data")) {
    args->ThrowTypeError("options.data must be an object");
    return false;
  }
  if (!ExtractOptionalDictionary(isolate, options, "globalProps",
                                 global_props) &&
      options.Has("globalProps")) {
    args->ThrowTypeError("options.globalProps must be an object");
    return false;
  }
  return true;
}

bool ExtractTemplateDataObject(v8::Isolate* isolate,
                               v8::Local<v8::Value> value,
                               v8::Local<v8::Object>* out) {
  if (!value->IsObject()) {
    return false;
  }
  v8::Local<v8::Object> obj = value.As<v8::Object>();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::String> key =
      v8::String::NewFromUtf8(isolate, "toObject").ToLocalChecked();
  v8::Local<v8::Value> to_object;
  if (obj->Get(context, key).ToLocal(&to_object) && to_object->IsFunction()) {
    v8::Local<v8::Function> fn = to_object.As<v8::Function>();
    v8::Local<v8::Value> result;
    if (!fn->Call(context, obj, 0, nullptr).ToLocal(&result)) {
      return false;
    }
    if (!result->IsObject()) {
      return false;
    }
    *out = result.As<v8::Object>();
    return true;
  }
  *out = obj;
  return true;
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

std::string ToDirectoryFileUrl(const base::FilePath& path) {
  std::string directory_url = ToFileUrl(path.DirName());
  if (!directory_url.empty() && directory_url.back() != '/') {
    directory_url.push_back('/');
  }
  return directory_url;
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
  auto it = registry.find(kLynxViewStateObserverName);
  if (it != registry.end()) {
    auto delegate = it->second.CreateDelegate();
    if (delegate) {
      lynx_view_state_observer_ = std::unique_ptr<LynxViewStateObserver>(
          reinterpret_cast<LynxViewStateObserver*>(delegate.release()));
    }
  } else {
    LOG(ERROR) << "LynxViewStateObserver not found in registry.";
  }
  LynxWindowManager::GetInstance()->RegisterLynxWindow(GetWeakPtr());
}

LynxWindow::~LynxWindow() {
  LynxWindowManager::GetInstance()->UnregisterLynxWindow(GetWeakPtr());
}

bool LynxWindow::ComputeRenderActive() const {
  return lynx_view_ != nullptr && window_ != nullptr && !window_->IsClosed() &&
         window_->IsVisible();
}

void LynxWindow::SyncRenderActiveState() {
  const bool render_active = ComputeRenderActive();
  if (render_active == last_render_active_) {
    return;
  }

  last_render_active_ = render_active;
  if (!lynx_view_) {
    return;
  }

  if (render_active) {
    lynx_view_->EnterForeground();
  } else {
    lynx_view_->EnterBackground();
  }
}

void LynxWindow::OnWindowBlur() {
  BaseWindow::OnWindowBlur();
}

void LynxWindow::OnWindowFocus() {
  if (lynx_view_) {
    lynx_view_->Focus();
  }
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
      const int w = win_rect.right - win_rect.left;
      const int h = win_rect.bottom - win_rect.top;
      MoveWindow(lynx_hwnd, win_rect.left, win_rect.top, w, h, FALSE);
    }
  }
#endif

  if (lynx_view_) {
    float width = window_->GetContentSize().width();
    float height = window_->GetContentSize().height();
    const float device_pixel_ratio = window_->GetDevicePixelRatio();
#if BUILDFLAG(IS_WIN)
    RECT cr{};
    ::GetClientRect(window_->GetNativeWindowHandle(), &cr);
    width = static_cast<float>(cr.right - cr.left) / device_pixel_ratio;
    height = static_cast<float>(cr.bottom - cr.top) / device_pixel_ratio;
#endif
    // During sizing, only adjust frame to keep visual coverage; defer metrics.
    lynx_view_->SetFrame(0, 0, width, height);
#if BUILDFLAG(IS_WIN)
    if (HWND lynx_hwnd =
            reinterpret_cast<HWND>(lynx_view_->GetNativeWindow())) {
      ::InvalidateRect(lynx_hwnd, nullptr, FALSE);
    }
#endif
    if (!window_->IsInSizeMove()) {
      lynx_view_->UpdateScreenMetrics(width, height, device_pixel_ratio);
    }
  }
  BaseWindow::OnWindowResize();
}

void LynxWindow::OnWindowResized() {
  if (lynx_view_) {
    const float dpr = window_->GetDevicePixelRatio();
    float width = window_->GetContentSize().width();
    float height = window_->GetContentSize().height();
#if BUILDFLAG(IS_WIN)
    RECT cr{};
    ::GetClientRect(window_->GetNativeWindowHandle(), &cr);
    width = static_cast<float>(cr.right - cr.left) / dpr;
    height = static_cast<float>(cr.bottom - cr.top) / dpr;
#endif
    lynx_view_->UpdateScreenMetrics(width, height, dpr);
    lynx_view_->SetFrame(0, 0, width, height);
#if BUILDFLAG(IS_WIN)
    if (HWND lynx_hwnd =
            reinterpret_cast<HWND>(lynx_view_->GetNativeWindow())) {
      ::InvalidateRect(lynx_hwnd, nullptr, FALSE);
    }
#endif
    lynx_view_->Focus();
  }
  BaseWindow::OnWindowResized();
}

void LynxWindow::OnWindowRestore() {
  SyncRenderActiveState();
  BaseWindow::OnWindowRestore();
}

void LynxWindow::OnWindowMinimize() {
  SyncRenderActiveState();
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
  SyncRenderActiveState();
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
  if (lynx_view_ && last_render_active_) {
    lynx_view_->EnterBackground();
    last_render_active_ = false;
  }
  auto lynx_view = std::move(lynx_view_);
  BaseWindow::CloseImmediately();

  if (lynx_view) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, lynx_view.release());
  }

  // Do not sent "unresponsive" event after window is closed.
  // window_unresponsive_closure_.Cancel();
}

void LynxWindow::Focus() {
  if (lynx_view_) {
    lynx_view_->Focus();
  }
  BaseWindow::Focus();
}

void LynxWindow::Blur() {
  BaseWindow::Blur();
}

void LynxWindow::EnsureLynxView() {
  if (lynx_view_) {
    return;
  }

  float width = window_->GetContentSize().width();
  float height = window_->GetContentSize().height();
  float device_pixel_ratio = window_->GetDevicePixelRatio();
#if BUILDFLAG(IS_WIN)
  RECT win_rect{};
  ::GetClientRect(window_->GetNativeWindowHandle(), &win_rect);
  width =
      static_cast<float>(win_rect.right - win_rect.left) / device_pixel_ratio;
  height =
      static_cast<float>(win_rect.bottom - win_rect.top) / device_pixel_ratio;
#endif

  LynxViewBuilder builder;
  builder.SetScreenSize(width, height, device_pixel_ratio)
      .SetFrame(0, 0, width, height)
      .SetParent(window_->GetNativeWindowHandle())
      .SetNodeIntegrationPreload(node_integration_preload_)
      .SetLynxWindow(GetWeakPtr());

  if (lynx_view_state_observer_) {
    lynx_view_state_observer_->OnPreLynxViewCreate(&builder);
  }
  lynx_view_ = builder.Build();
  lynx_view_->SetClient(weak_factory_.GetWeakPtr());
  SyncRenderActiveState();

  if (data_str_.has_value() && global_props_.has_value()) {
    lynx_view_->UpdateData(data_str_.value(), global_props_.value());
    data_str_.reset();
    global_props_.reset();
  }
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
  SyncRenderActiveState();
  BaseWindow::OnWindowShow();
}

void LynxWindow::OnWindowHide() {
  SyncRenderActiveState();
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

  gin_helper::Dictionary data;
  gin_helper::Dictionary global_props;
  if (!ExtractLoadDataOptions(args, &data, &global_props)) {
    return false;
  }

  EnsureLynxView();
  auto data_json = ConvertDictionaryToJsonString(data);
  if (!data_json.has_value()) {
    return false;
  }
  auto props_json = ConvertDictionaryToJsonString(global_props);
  if (!props_json.has_value()) {
    return false;
  }

  SetTemplateResourceBaseFromFile(local_path);
  lynx_view_->LoadFile(local_path.AsUTF8Unsafe(), data_json.value(),
                       global_props.IsEmptyObject() ? "" : props_json.value());
  return true;
}

bool LynxWindow::LoadUrl(const std::string& url, gin::Arguments* args) {
  gin_helper::Dictionary data;
  gin_helper::Dictionary global_props;
  if (!ExtractLoadDataOptions(args, &data, &global_props)) {
    return false;
  }

  EnsureLynxView();

  auto data_json = ConvertDictionaryToJsonString(data);
  if (!data_json.has_value()) {
    return false;
  }
  auto props_json = ConvertDictionaryToJsonString(global_props);
  if (!props_json.has_value()) {
    return false;
  }

  SetTemplateResourceBaseFromUrl(url);
  lynx_view_->LoadURL(url, data_json.value(),
                      global_props.IsEmpty() ? "" : props_json.value());
  return true;
}

bool LynxWindow::LoadBundle(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  v8::HandleScope handle_scope(isolate);

  api::LynxTemplateBundle* wrapper = nullptr;
  if (!args->GetNext(&wrapper) || wrapper == nullptr) {
    args->ThrowTypeError("loadBundle requires a LynxTemplateBundle instance");
    return false;
  }

  gin_helper::Dictionary data;
  gin_helper::Dictionary global_props;
  if (!ExtractLoadDataOptions(args, &data, &global_props)) {
    return false;
  }

  EnsureLynxView();

  auto data_json = ConvertDictionaryToJsonString(data);
  if (!data_json.has_value()) {
    return false;
  }
  auto props_json = ConvertDictionaryToJsonString(global_props);
  if (!props_json.has_value()) {
    return false;
  }

  current_resource_base_url_.reset();
  current_resource_base_is_file_ = false;
  lynx_view_->LoadBundle(wrapper->GetImpl(), data_json.value(),
                         global_props.IsEmpty() ? "" : props_json.value());
  return true;
}

std::string LynxWindow::ResolveResourceUrl(
    const std::string& resource_url) const {
  if (resource_url.empty()) {
    return resource_url;
  }

  GURL resource(resource_url);
  if (resource.is_valid() && resource.has_scheme()) {
    return resource.spec();
  }

  if (!current_resource_base_url_.has_value()) {
    return resource_url;
  }

  GURL base_url(current_resource_base_url_.value());
  if (!base_url.is_valid()) {
    return resource_url;
  }

  std::string relative = resource_url;
  if (current_resource_base_is_file_) {
    while (!relative.empty() && relative.front() == '/') {
      relative.erase(relative.begin());
    }
  }

  GURL resolved = base_url.Resolve(relative);
  return resolved.is_valid() ? resolved.spec() : resource_url;
}

void LynxWindow::SetTemplateResourceBaseFromFile(const base::FilePath& path) {
  current_resource_base_url_ = ToDirectoryFileUrl(path);
  current_resource_base_is_file_ = true;
}

void LynxWindow::SetTemplateResourceBaseFromUrl(const std::string& url) {
  current_resource_base_url_ = url;
  current_resource_base_is_file_ = false;
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

bool LynxWindow::UpdateMetaData(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  v8::HandleScope handle_scope(isolate);

  gin_helper::Dictionary meta;
  if (!args->GetNext(&meta)) {
    args->ThrowTypeError("updateMetaData requires a LynxUpdateMeta instance");
    return false;
  }

  v8::Local<v8::Value> update_data_value;
  if (!meta.Get("updateData", &update_data_value)) {
    args->ThrowTypeError("updateMetaData requires meta.updateData");
    return false;
  }
  v8::Local<v8::Value> global_props_value;
  if (!meta.Get("globalProps", &global_props_value)) {
    args->ThrowTypeError("updateMetaData requires meta.globalProps");
    return false;
  }

  v8::Local<v8::Object> update_data_object;
  if (!ExtractTemplateDataObject(isolate, update_data_value,
                                 &update_data_object)) {
    args->ThrowTypeError(
        "updateMetaData requires meta.updateData as an object");
    return false;
  }
  v8::Local<v8::Object> global_props_object;
  if (!ExtractTemplateDataObject(isolate, global_props_value,
                                 &global_props_object)) {
    args->ThrowTypeError(
        "updateMetaData requires meta.globalProps as an object");
    return false;
  }

  gin_helper::Dictionary update_data_dict(isolate, update_data_object);
  gin_helper::Dictionary global_props_dict(isolate, global_props_object);
  auto update_data_json = ConvertDictionaryToJsonString(update_data_dict);
  if (!update_data_json.has_value()) {
    return false;
  }
  auto global_props_json = ConvertDictionaryToJsonString(global_props_dict);
  if (!global_props_json.has_value()) {
    return false;
  }

  std::string data_json = std::move(update_data_json).value();
  std::string props_json = std::move(global_props_json).value();
  auto impl = std::make_shared<lynxtron::LynxUpdateMeta>();
  impl->SetUpdateData(data_json);
  impl->SetGlobalProps(props_json);

  if (!lynx_view_) {
    data_str_ = std::move(data_json);
    global_props_ = std::move(props_json);
    return true;
  }

  lynx_view_->UpdateData(std::move(impl));
  return true;
}

void LynxWindow::OnPageStart(std::string_view url) {
  // Emit("on-page-start", url);
  if (lynx_view_state_observer_) {
    lynx_view_state_observer_->SetInstanceId(
        reinterpret_cast<int64_t>(lynx_view_.get()));
    lynx_view_state_observer_->OnPageStart(url);
  }
}

/**
 * page load success
 */
void LynxWindow::OnLoadSuccess() {
  Emit("ready-to-show");
}

/**
 * first screen layout complete
 */
void LynxWindow::OnFirstScreen() {
  Emit("on-first-screen");
}

void LynxWindow::OnDestroy() {}

/**
 * notify JS Runtime initialization complete
 */
void LynxWindow::OnRuntimeReady() {}

void LynxWindow::OnReceivedError(int error_code, std::string_view message) {
  if (lynx_view_state_observer_) {
    lynx_view_state_observer_->OnReceivedError(error_code, message);
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

void LynxWindow::OnFrameTiming(int64_t frame_start_time_in_ns,
                               int64_t frame_finish_time_in_ns) {
  if (!enable_fps_monitor_) {
    return;
  }
  last_frame_timings_.emplace_back(std::initializer_list<int64_t>{
      frame_start_time_in_ns, frame_finish_time_in_ns});
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
  if (!Application::Get()->is_ready()) {
    thrower.ThrowError("Cannot create LynxWindow before app is ready");
    return nullptr;
  }

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
      .SetMethod("loadBundle", &LynxWindow::LoadBundle)
      .SetMethod("updateMetaData", &LynxWindow::UpdateMetaData)
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
