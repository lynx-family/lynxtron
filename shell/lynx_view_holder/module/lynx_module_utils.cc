// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/lynx_view_holder/module/lynx_module_utils.h"

#include "v8.h"

// #include "base/include/log/logging.h"
#include "third_party/napi/include/napi_env_v8.h"

namespace lynxtron {

v8::Local<v8::Value> DeserializeValue(v8::Isolate* isolate,
                                      v8::Local<v8::Context> context,
                                      const V8SerializerValue& value) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::Context::Scope context_scope(context);

  v8::ValueDeserializer deserializer(isolate, value.data(), value.size());

  if (!deserializer.ReadHeader(context).FromMaybe(false)) {
    // TODO(liting) LOG!!!!
    return v8::Null(isolate);
  }

  v8::Local<v8::Value> js_value =
      deserializer.ReadValue(context).ToLocalChecked();
  return js_value;
}

V8SerializerValue SerializeValue(v8::Isolate* isolate,
                                 v8::Local<v8::Context> context,
                                 v8::Local<v8::Value> value) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::Context::Scope context_scope(context);

  v8::ValueSerializer serializer(isolate);
  serializer.WriteHeader();
  v8::Maybe<bool> ok = serializer.WriteValue(context, value);
  if (!ok.FromMaybe(false)) {
    // TODO(liting) LOG!!!!
    // LOG(ERROR) << "SerializeValue JSB Param failed, write value failed";
    return {};
  }

  auto data = serializer.Release();
  return V8SerializerValue(data.first, data.first + data.second);
}
}  // namespace lynxtron
