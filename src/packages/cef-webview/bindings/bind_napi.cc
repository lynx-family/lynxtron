// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#define lynx_EXPORTS

#include <napi.h>

#include "platform/embedder/plugin/cef/include/cef_extension_module_creator.h"

napi_value Initialize(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() != 1 || !info[0].IsObject() || info[0].IsArray()) {
    Napi::TypeError::New(env, "Expected resolved CEF storage settings")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  auto options = info[0].As<Napi::Object>();
  auto root_value = options.Get("rootCachePath");
  auto cache_value = options.Get("cachePath");
  if (!root_value.IsString() || !cache_value.IsString()) {
    Napi::TypeError::New(env, "CEF paths must be strings")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  auto root = root_value.As<Napi::String>().Utf8Value();
  auto cache = cache_value.As<Napi::String>().Utf8Value();
  if (root.empty() || root.find('\0') != std::string::npos ||
      cache.find('\0') != std::string::npos) {
    Napi::TypeError::New(env, "Invalid CEF storage paths")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  cef_extension_settings_t settings{root.c_str(), cache.c_str()};
  bool result = cef_extension_module_initialize_with_settings(&settings);
  auto result_value = Napi::Boolean::New(env, result);
  return result_value;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("initialize", Napi::Function::New(env, Initialize));
  return exports;
}

NODE_API_MODULE(lynx_ex, Init)
