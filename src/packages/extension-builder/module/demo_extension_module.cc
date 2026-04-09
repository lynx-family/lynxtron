// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "module/demo_extension_module.h"

#include "module/demo_view.h"

namespace extension {
namespace {

const uint64_t kDemoExtensionModuleID =
    reinterpret_cast<uint64_t>(&kDemoExtensionModuleID);

Napi::Value CallByLynxJS(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  void* data;
  lynx::pub::LynxExtensionModule::GetNapiInstanceData(
      env, kDemoExtensionModuleID, &data);
  auto* module = reinterpret_cast<DemoExtensionModule*>(data);

  const std::string& module_result = module->CallByLynxJS();

  return Napi::String::New(env, module_result.c_str(), module_result.length());
}

Napi::Value DemoExtensionModuleMethodsBinder(Napi::Env env,
                                             Napi::Value exports,
                                             const char* module_name,
                                             DemoExtensionModule& module) {
  if (!exports.IsObject()) {
    return exports;
  }
  Napi::Object exports_obj = exports.As<Napi::Object>();
  exports_obj.Set("callByLynxJS",
                  Napi::Function::New(env, CallByLynxJS, "callByLynxJS"));
  return exports;
}
}  // namespace

void DemoExtensionModule::OnLynxViewCreate(lynx_view_t* lynx_view) {
  lynx_view_register_native_view(lynx_view, "demo-view", &demo_view_create_view,
                                 nullptr);
}
void DemoExtensionModule::OnLynxViewDestroy() {
  // do something
}
void DemoExtensionModule::OnRuntimeInit() {
  // do something
}
void DemoExtensionModule::OnRuntimeAttach(
    Napi::Env env,
    std::unique_ptr<lynx::pub::VSyncObserver> vsync_observer) {
  lynx::pub::LynxExtensionModule::SetNapiInstanceData(
      env, kDemoExtensionModuleID, this,
      [](Napi::Env env, void* data, void* hint) {}, nullptr);
}
void DemoExtensionModule::OnRuntimeReady(Napi::Env env,
                                         Napi::Value lynx,
                                         const char* url) {
  // do something
}
void DemoExtensionModule::OnRuntimeDetach() {
  // do something
}
void DemoExtensionModule::OnEnterForeground() {
  // do something
}
void DemoExtensionModule::OnEnterBackground() {
  // do something
}
void DemoExtensionModule::Destroy() {
  // do something
}

std::string DemoExtensionModule::CallByLynxJS() {
  static uint64_t call_count = 0;

  std::string result = "Click count:" + std::to_string(call_count++);

  return result;
}

}  // namespace extension

LYNX_EXTERN_C lynx_extension_module_t*
demo_extension_module_create_extension_module(void* opaque) {
  auto* module = new extension::DemoExtensionModule();
  lynx_extension_module_t* c_module =
      lynx_extension_module_create_with_finalizer(
          module, [](lynx_extension_module_t* m, void* user_data) {
            if (user_data) {
              delete reinterpret_cast<extension::DemoExtensionModule*>(
                  user_data);
            }
          });

  module->SetCModule(c_module);
  module->SetNapiModuleCreator(&extension::DemoExtensionModuleMethodsBinder);
  return c_module;
}
