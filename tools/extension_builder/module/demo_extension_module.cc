// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "module/demo_extension_module.h"

#include "module/demo_view.h"
#include "third_party/napi/include/primjs_napi_defines.h"

namespace extension {
namespace {
#define NAPI_CREATE_FUNCTION(env, exports, function_name, function)           \
  {                                                                           \
    napi_value native_result;                                                 \
    env->napi_create_function(env, function_name, strlen(function_name),      \
                              function, nullptr, &native_result);             \
    env->napi_set_named_property(env, exports, function_name, native_result); \
  }

const uint64_t kDemoExtensionModuleID =
    reinterpret_cast<uint64_t>(&kDemoExtensionModuleID);

napi_value CallByLynxJS(napi_env env, napi_callback_info info) {
  void* data;
  lynx_napi_get_instance_data(env, kDemoExtensionModuleID, &data);
  auto* module = reinterpret_cast<DemoExtensionModule*>(data);

  const std::string& module_result = module->CallByLynxJS();

  napi_value result;
  env->napi_create_string_utf8(env, module_result.c_str(),
                               module_result.length(), &result);
  return result;
}

napi_value DemoExtensionModuleMethodsBinder(napi_env env,
                                            napi_value exports,
                                            const char* module_name,
                                            void* opaque) {
  NAPI_CREATE_FUNCTION(env, exports, "callByLynxJS", CallByLynxJS)
  return exports;
}
}  // namespace

void DemoExtensionModule::OnLynxViewCreate(lynx_view_t* lynx_view) {
  lynx_view_register_native_view(lynx_view, "demo-view", &demo_view_create_view,
                                 lynx_view);
}
void DemoExtensionModule::OnLynxViewDestroy() {
  // do something
}
void DemoExtensionModule::OnRuntimeInit() {
  // do something
}
void DemoExtensionModule::OnRuntimeAttach(
    napi_env env,
    std::unique_ptr<lynx::pub::VSyncObserver> vsync_observer) {
  lynx_napi_set_instance_data(env, kDemoExtensionModuleID, this, nullptr,
                              nullptr);
}
void DemoExtensionModule::OnRuntimeReady(napi_env env,
                                         napi_value lynx,
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

  std::string result = "点击次数:" + std::to_string(call_count++);

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
