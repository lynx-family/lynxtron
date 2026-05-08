
// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/api/lynx_view/module/lynx_hybrid_monitor_module.h"

#include <string>

#include "shell/api/api_lynx_window.h"
#include "shell/api/lynx_view/module/lynx_native_module.h"
#include "v8.h"

#ifdef USE_WEAK_SUFFIX_NAPI
#include "third_party/weak-node-api/headers/weak_napi_defines.h"
#endif

namespace lynxtron {
constexpr const char* const kModuleName = "hybridMonitor";

class LynxHybridMonitorModuleImpl : public LynxNativeModule {
 public:
  LynxHybridMonitorModuleImpl(base::WeakPtr<api::LynxWindow> lynx_window);
  ~LynxHybridMonitorModuleImpl() override = default;

 private:
  static void ReportJSError(LynxNativeModule* module,
                            v8::Isolate* isolate,
                            v8::Local<v8::Context> ctx,
                            v8::Local<v8::Value> const* argv,
                            int argc);
  static void CustomReport(LynxNativeModule* module,
                           v8::Isolate* isolate,
                           v8::Local<v8::Context> ctx,
                           v8::Local<v8::Value> const* argv,
                           int argc);
  static void ConfigJsBase(LynxNativeModule* module,
                           v8::Isolate* isolate,
                           v8::Local<v8::Context> ctx,
                           v8::Local<v8::Value> const* argv,
                           int argc);

  base::WeakPtr<api::LynxWindow> lynx_window_;
};

LynxHybridMonitorModuleImpl::LynxHybridMonitorModuleImpl(
    base::WeakPtr<api::LynxWindow> lynx_window)
    : lynx_window_(lynx_window) {
  RegisterMethod("reportJSError", LynxHybridMonitorModuleImpl::ReportJSError);
  RegisterMethod("customReport", LynxHybridMonitorModuleImpl::CustomReport);
  RegisterMethod("config", LynxHybridMonitorModuleImpl::ConfigJsBase);
}

void LynxHybridMonitorModuleImpl::ReportJSError(
    LynxNativeModule* module,
    v8::Isolate* isolate,
    v8::Local<v8::Context> ctx,
    v8::Local<v8::Value> const* argv,
    int argc) {
  if (argc != 2) {
    return;
  }

  auto* lynx_windows =
      static_cast<LynxHybridMonitorModuleImpl*>(module)->lynx_window_.get();
  if (!lynx_windows) {
    return;
  }

  v8::Local<v8::String> error_info;
  if (!v8::JSON::Stringify(ctx, argv[0]).ToLocal(&error_info)) {
    return;
  }

  v8::String::Utf8Value utf8_value(isolate, error_info);
  std::string error_info_str = *utf8_value ? *utf8_value : "";
  lynx_windows->ReportJSError(error_info_str);
}

void LynxHybridMonitorModuleImpl::CustomReport(LynxNativeModule* module,
                                               v8::Isolate* isolate,
                                               v8::Local<v8::Context> ctx,
                                               v8::Local<v8::Value> const* argv,
                                               int argc) {
  if (argc != 2) {
    return;
  }

  auto* lynx_windows =
      static_cast<LynxHybridMonitorModuleImpl*>(module)->lynx_window_.get();
  if (!lynx_windows) {
    return;
  }

  v8::Local<v8::String> custom_data;
  if (!v8::JSON::Stringify(ctx, argv[0]).ToLocal(&custom_data)) {
    return;
  }

  v8::String::Utf8Value utf8_value(isolate, custom_data);
  std::string custom_data_str = *utf8_value ? *utf8_value : "";
  lynx_windows->CustomReport(custom_data_str);
}

void LynxHybridMonitorModuleImpl::ConfigJsBase(LynxNativeModule* module,
                                               v8::Isolate* isolate,
                                               v8::Local<v8::Context> ctx,
                                               v8::Local<v8::Value> const* argv,
                                               int argc) {
  if (argc != 2) {
    return;
  }

  auto* lynx_windows =
      static_cast<LynxHybridMonitorModuleImpl*>(module)->lynx_window_.get();
  if (!lynx_windows) {
    return;
  }

  v8::Local<v8::String> custom_data;
  if (!v8::JSON::Stringify(ctx, argv[0]).ToLocal(&custom_data)) {
    return;
  }

  v8::String::Utf8Value utf8_value(isolate, custom_data);
  std::string custom_data_str = *utf8_value ? *utf8_value : "";
  lynx_windows->ConfigJSBase(custom_data_str);
}

void RegisterLynxHybridMonitorModuleToLynxView(
    lynx_view_builder_t* builder,
    base::WeakPtr<api::LynxWindow> lynx_window) {
  LynxNativeModule::RegisterLynxNodeModule(
      builder, kModuleName, new LynxHybridMonitorModuleImpl(lynx_window));
}

}  // namespace lynxtron
