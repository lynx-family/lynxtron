// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_devtool.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"
#include "lynx/platform/embedder/public/lynx_env.h"
#include "shell/common/gin_converters/callback_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/error_thrower.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/global_thread.h"
#include "shell/common/node_includes.h"

#ifdef USE_WEAK_SUFFIX_NAPI
#include "third_party/weak-node-api/headers/weak_napi_defines.h"
#endif

namespace lynxtron {
namespace api {
namespace {

using OpenCardCallback = base::RepeatingCallback<void(std::string)>;

OpenCardCallback& GetOpenCardCallback() {
  static base::NoDestructor<OpenCardCallback> callback;
  return *callback;
}

void ClearOpenCardCallback() {
  GetOpenCardCallback().Reset();
  lynx::pub::LynxEnv::GetInstance().SetOpenCardCallback(nullptr, nullptr);
}

void RunOpenCardCallback(std::string url) {
  DCHECK_CURRENTLY_ON(GlobalThread::UI);
  auto& callback = GetOpenCardCallback();
  if (callback.is_null()) {
    return;
  }
  callback.Run(url);
}

void HandleOpenCard(void* /*user_data*/, const char* url) {
  GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunOpenCardCallback, std::string(url ? url : "")));
}

}  // namespace

Devtool* Devtool::GetInstance() {
  static ::base::NoDestructor<Devtool> devtool;
  return devtool.get();
}

gin_helper::Handle<Devtool> Devtool::Create(v8::Isolate* isolate) {
  return gin_helper::CreateHandle(isolate, GetInstance());
}

gin::DeprecatedWrapperInfo Devtool::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder Devtool::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::ObjectTemplateBuilder(isolate)
      .SetMethod("setDevToolEnabled", &Devtool::SetDevToolEnabled)
      .SetMethod("isDevtoolEnabled", &Devtool::IsDevtoolEnabled)
      .SetMethod("setLogboxEnabled", &Devtool::SetLogboxEnabled)
      .SetMethod("isLogboxEnabled", &Devtool::IsLogboxEnabled)
      .SetMethod("setOpenCardCallback", &Devtool::SetOpenCardCallback)
      .SetMethod("connectDevtool", &Devtool::ConnectDevtool);
}

const char* Devtool::GetTypeName() {
  return "devtool";
}

Devtool::Devtool() = default;
Devtool::~Devtool() = default;

void Devtool::SetDevToolEnabled(bool enabled) {
  lynx::pub::LynxEnv::GetInstance().SetDevtoolEnabled(enabled);
}

bool Devtool::IsDevtoolEnabled() {
  return lynx::pub::LynxEnv::GetInstance().IsDevtoolEnabled();
}

void Devtool::SetLogboxEnabled(bool enable) {
  lynx::pub::LynxEnv::GetInstance().SetLogboxEnabled(enable);
}

bool Devtool::IsLogboxEnabled() {
  return lynx::pub::LynxEnv::GetInstance().IsLogboxEnabled();
}

void Devtool::SetOpenCardCallback(gin::Arguments* args) {
  if (!args || args->Length() == 0) {
    ClearOpenCardCallback();
    return;
  }

  v8::Local<v8::Value> handler;
  if (!args->GetNext(&handler) || handler->IsNullOrUndefined()) {
    ClearOpenCardCallback();
    return;
  }

  if (!handler->IsFunction()) {
    gin_helper::ErrorThrower(args->isolate())
        .ThrowTypeError("callback must be a function or null");
    return;
  }

  OpenCardCallback callback;
  if (!gin::ConvertFromV8(args->isolate(), handler, &callback)) {
    gin_helper::ErrorThrower(args->isolate())
        .ThrowTypeError("failed to convert callback");
    return;
  }

  GetOpenCardCallback() = std::move(callback);
  lynx::pub::LynxEnv::GetInstance().SetOpenCardCallback(&HandleOpenCard,
                                                        nullptr);
}

void Devtool::ConnectDevtool(std::string url) {
  lynx::pub::LynxEnv::GetInstance().ConnectDevtool(url.c_str());
}

}  // namespace api
}  // namespace lynxtron

namespace {

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  gin_helper::Dictionary dict(isolate, exports);
  dict.Set("devtool", lynxtron::api::Devtool::Create(isolate));
}

}  // namespace

#ifdef USE_WEAK_SUFFIX_NAPI
#include "third_party/weak-node-api/headers/weak_napi_undefs.h"
#endif

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_binding_devtool, Initialize)
