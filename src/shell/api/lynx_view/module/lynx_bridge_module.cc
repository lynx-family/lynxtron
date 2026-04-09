
// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/api/lynx_view/module/lynx_bridge_module.h"

#include <string>

#include "shell/api/api_lynx_window.h"
#include "shell/api/lynx_view/module/lynx_native_module.h"
#include "v8.h"

#ifdef USE_WEAK_SUFFIX_NAPI
#include "third_party/weak-node-api/headers/weak_napi_defines.h"
#endif

namespace lynxtron {
constexpr const char* const kModuleName = "bridge";

class LynxBridgeModuleImpl : public LynxNativeModule {
 public:
  LynxBridgeModuleImpl(base::WeakPtr<api::LynxWindow> lynx_window);
  ~LynxBridgeModuleImpl() override = default;

 private:
  // on main thread
  static void Send(LynxNativeModule* module,
                   v8::Isolate* isolate,
                   v8::Local<v8::Context> ctx,
                   v8::Local<v8::Value> const* argv,
                   int argc);
  static void Call(LynxNativeModule* module,
                   v8::Isolate* isolate,
                   v8::Local<v8::Context> ctx,
                   v8::Local<v8::Value> const* argv,
                   int argc);

  base::WeakPtr<api::LynxWindow> lynx_window_;
};

LynxBridgeModuleImpl::LynxBridgeModuleImpl(
    base::WeakPtr<api::LynxWindow> lynx_window)
    : lynx_window_(lynx_window) {
  RegisterMethod("call", LynxBridgeModuleImpl::Call);
  RegisterMethod("send", LynxBridgeModuleImpl::Send);
}
// static, on main thread
void LynxBridgeModuleImpl::Call(LynxNativeModule* module,
                                v8::Isolate* isolate,
                                v8::Local<v8::Context> ctx,
                                v8::Local<v8::Value> const* argv,
                                int argc) {
  if (argc != 3) {
    return;
  }

  auto* lynx_windows =
      static_cast<LynxBridgeModuleImpl*>(module)->lynx_window_.get();
  if (!lynx_windows) {
    return;
  }

  lynx_windows->EmitWithoutEvent("-lynx-invoke", argv[2], argv[0], argv[1]);
}

// static, on main thread
void LynxBridgeModuleImpl::Send(LynxNativeModule* module,
                                v8::Isolate* isolate,
                                v8::Local<v8::Context> ctx,
                                v8::Local<v8::Value> const* argv,
                                int argc) {
  if (argc != 2) {
    return;
  }

  auto* lynx_windows =
      static_cast<LynxBridgeModuleImpl*>(module)->lynx_window_.get();
  if (!lynx_windows) {
    return;
  }

  lynx_windows->EmitWithoutEvent("-lynx-message", argv[0], argv[1]);
}

void RegisterLynxBridgeModuleToLynxView(
    lynx_view_builder_t* builder,
    base::WeakPtr<api::LynxWindow> lynx_window) {
  LynxNativeModule::RegisterLynxNodeModule(
      builder, kModuleName, new LynxBridgeModuleImpl(lynx_window));
}

}  // namespace lynxtron
