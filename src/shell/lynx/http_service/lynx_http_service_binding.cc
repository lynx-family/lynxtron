// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/lynx/http_service/lynx_http_service_binding.h"

#include "base/no_destructor.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/error_thrower.h"
#include "shell/common/node_includes.h"
#include "v8.h"

namespace lynxtron {
namespace {

v8::Global<v8::Function>& GetRequestHandler() {
  static base::NoDestructor<v8::Global<v8::Function>> request_handler;
  return *request_handler;
}

void SetRequestHandler(gin_helper::ErrorThrower thrower,
                       v8::Local<v8::Value> handler) {
  if (!handler->IsFunction()) {
    thrower.ThrowTypeError("handler must be a function");
    return;
  }
  GetRequestHandler().Reset(thrower.isolate(), handler.As<v8::Function>());
}

}  // namespace

bool InvokeLynxHttpRequestHandler(v8::Isolate* isolate,
                                  v8::Local<v8::Value> request,
                                  v8::Local<v8::Function> reply) {
  auto& request_handler = GetRequestHandler();
  if (request_handler.IsEmpty()) {
    return false;
  }

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> args[] = {request, reply};
  auto result = request_handler.Get(isolate)->Call(
      isolate->GetCurrentContext(), v8::Undefined(isolate), 2, args);
  return !try_catch.HasCaught() && !result.IsEmpty();
}

}  // namespace lynxtron

namespace {

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  gin_helper::Dictionary dict{isolate, exports};
  dict.SetMethod("setRequestHandler", &lynxtron::SetRequestHandler);
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_binding_lynx_http_service,
                                  Initialize)
