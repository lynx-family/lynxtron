// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include <napi.h>

#include "lynxtron/capi/lynx_extension_module_types_capi.h"

extern "C" lynx_extension_module_t*
demo_extension_module_create_extension_module(void* opaque);

typedef struct lynx_extension_module_creator_api_t {
  extension_module_creator create_module_func;
} lynx_extension_module_creator_api_t;

napi_value GetModuleCreator(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  auto* creator_api = new lynx_extension_module_creator_api_t{
      .create_module_func = demo_extension_module_create_extension_module,
  };

  auto result = Napi::Object::New(env);
  result.Set("name", Napi::String::New(env, "DemoExtensionModule"));

  result.Set("creatorModuleFunc",
             Napi::External<void>::New(
                 env, creator_api, [](Napi::Env env, void* data) {
                   delete (lynx_extension_module_creator_api_t*)data;
                 }));
  result.Set("isLazyCreate", Napi::Boolean::New(env, false));
  result.Set("opaque", Napi::External<void>::New(env, nullptr));

  return result;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("getModuleCreator", Napi::Function::New(env, GetModuleCreator));
  return exports;
}

NODE_API_MODULE(lynx_ex, Init)
