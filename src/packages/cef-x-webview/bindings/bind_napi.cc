// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#define lynx_EXPORTS

#include <napi.h>

extern "C" bool cef_extension_module_initialize();

napi_value Initialize(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  bool result = cef_extension_module_initialize();
  auto result_value = Napi::Boolean::New(env, result);
  return result_value;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("initialize", Napi::Function::New(env, Initialize));
  return exports;
}

NODE_API_MODULE(lynx_ex, Init)
