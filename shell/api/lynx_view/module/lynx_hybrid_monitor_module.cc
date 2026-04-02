
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
  RegisterMethod("ReportJSError", LynxHybridMonitorModuleImpl::ReportJSError);
  RegisterMethod("CustomReport", LynxHybridMonitorModuleImpl::CustomReport);
  RegisterMethod("ConfigJsBase", LynxHybridMonitorModuleImpl::ConfigJsBase);
}

void LynxHybridMonitorModuleImpl::ReportJSError(
    LynxNativeModule* module,
    v8::Isolate* isolate,
    v8::Local<v8::Context> ctx,
    v8::Local<v8::Value> const* argv,
    int argc) {
  // TODO(chennengshi) Add implementation
}

void LynxHybridMonitorModuleImpl::CustomReport(LynxNativeModule* module,
                                               v8::Isolate* isolate,
                                               v8::Local<v8::Context> ctx,
                                               v8::Local<v8::Value> const* argv,
                                               int argc) {
  // TODO(chennengshi) Add implementation
}

void LynxHybridMonitorModuleImpl::ConfigJsBase(LynxNativeModule* module,
                                               v8::Isolate* isolate,
                                               v8::Local<v8::Context> ctx,
                                               v8::Local<v8::Value> const* argv,
                                               int argc) {
  // TODO(chennengshi) Add implementation
}

void RegisterLynxHybridMonitorModuleToLynxView(
    lynx_view_builder_t* builder,
    base::WeakPtr<api::LynxWindow> lynx_window) {
  LynxNativeModule::RegisterLynxNodeModule(
      builder, kModuleName, new LynxHybridMonitorModuleImpl(lynx_window));
}

}  // namespace lynxtron
