// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <napi.h>

#include "lynx/platform/embedder/public/capi/lynx_extension_module_types_capi.h"
#include "plugin/cef/cef_extension_module.h"

typedef struct lynx_extension_module_creator_api_t {
  extension_module_creator create_module_func;
} lynx_extension_module_creator_api_t;

napi_value GetExtensionConfig(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  auto* creator_api = new lynx_extension_module_creator_api_t{
      .create_module_func = cef_extension_module_create_extension_module,
  };

  auto result = Napi::Object::New(env);
  result.Set("name", Napi::String::New(env, "CEFExtensionModule"));

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
  exports.Set("getCefExtensionConfig",
              Napi::Function::New(env, GetExtensionConfig));
  return exports;
}

NODE_API_MODULE(cef_webview_module, Init)
