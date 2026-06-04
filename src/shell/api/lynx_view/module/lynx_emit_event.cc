// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/module/lynx_emit_event.h"

#include <utility>

#include "gin/data_object_builder.h"
#include "gin/object_template_builder.h"
#include "shell/app/javascript_environment.h"
#include "v8.h"

namespace lynxtron {

gin::DeprecatedWrapperInfo LynxEmitEvent::kWrapperInfo = {
    gin::kEmbedderNativeGin};

LynxEmitEvent::LynxEmitEvent(InvokeCallback callback)
    : callback_(std::move(callback)) {}

LynxEmitEvent::~LynxEmitEvent() {
  if (callback_) {
    v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
    // If there's no current context, it means we're shutting down, so we don't
    // need to send an event.
    if (!isolate->GetCurrentContext().IsEmpty()) {
      v8::HandleScope scope(isolate);

      v8::Local<v8::Object> data = v8::Object::New(isolate);
      v8::Local<v8::Context> context = isolate->GetCurrentContext();

      if (data->Set(context, v8::String::NewFromUtf8Literal(isolate, "error"),
                    v8::String::NewFromUtf8Literal(isolate,
                                                   "reply was never sent"))
              .IsNothing()) {
        // Failed to set error property, skip sending reply.
      }
      SendReply(isolate, data);
    }
  }
}

gin_helper::Handle<LynxEmitEvent> LynxEmitEvent::Create(
    v8::Isolate* isolate,
    InvokeCallback callback) {
  return gin_helper::CreateHandle(isolate,
                                  new LynxEmitEvent(std::move(callback)));
}

gin::ObjectTemplateBuilder LynxEmitEvent::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin_helper::DeprecatedWrappable<
             LynxEmitEvent>::GetObjectTemplateBuilder(isolate)
      .SetMethod("sendReply", &LynxEmitEvent::SendReply);
}

bool LynxEmitEvent::SendReply(v8::Isolate* isolate, v8::Local<v8::Value> arg) {
  if (!callback_) {
    return false;
  }

  std::move(callback_)(isolate, arg);
  return true;
}

}  // namespace lynxtron
