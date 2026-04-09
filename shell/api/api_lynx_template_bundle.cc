// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_lynx_template_bundle.h"

#include <memory>
#include <string>
#include <utility>

#include "lynx/platform/embedder/public/lynx_template_bundle.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/error_thrower.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/node_includes.h"

namespace lynxtron::api {

gin::DeprecatedWrapperInfo LynxTemplateBundle::kWrapperInfo = {
    gin::kEmbedderNativeGin};

LynxTemplateBundle::LynxTemplateBundle(
    std::shared_ptr<lynx::pub::LynxTemplateBundle> bundle)
    : bundle_(std::move(bundle)) {}

void LynxTemplateBundle::FillObjectTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> prototype) {
  gin_helper::ObjectTemplateBuilder(isolate, prototype)
      .SetMethod("isValid", &LynxTemplateBundle::isValid)
      .SetMethod("getErrorMessage", &LynxTemplateBundle::getErrorMessage);
}

const char* LynxTemplateBundle::GetTypeName() {
  return GetClassName();
}

bool LynxTemplateBundle::isValid() const {
  return bundle_ && bundle_->IsValid();
}

std::string LynxTemplateBundle::getErrorMessage() const {
  if (!bundle_) {
    return "";
  }
  return bundle_->GetErrorMessage();
}

// static
LynxTemplateBundle* LynxTemplateBundle::New(gin_helper::ErrorThrower thrower,
                                            v8::Local<v8::Value> buffer_like) {
  if ((!node::Buffer::HasInstance(buffer_like)) &&
      !buffer_like->IsArrayBufferView()) {
    thrower.ThrowTypeError(
        "Expected Buffer or any ArrayBufferView for LynxTemplateBundle");
    return nullptr;
  }

  uint8_t* data_ptr = nullptr;
  size_t data_len = 0;
  std::shared_ptr<v8::BackingStore> store;
  if (node::Buffer::HasInstance(buffer_like)) {
    data_ptr = reinterpret_cast<uint8_t*>(node::Buffer::Data(buffer_like));
    data_len = node::Buffer::Length(buffer_like);
  } else {
    auto view = buffer_like.As<v8::ArrayBufferView>();
    store = view->Buffer()->GetBackingStore();
    data_ptr = static_cast<uint8_t*>(store->Data()) + view->ByteOffset();
    data_len = view->ByteLength();
  }

  auto bundle = std::make_shared<lynx::pub::LynxTemplateBundle>(
      data_ptr, data_len, nullptr, nullptr);
  return new LynxTemplateBundle(std::move(bundle));
}

namespace {

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* const isolate = context->GetIsolate();
  gin_helper::Dictionary dict(isolate, exports);
  dict.Set("LynxTemplateBundle",
           LynxTemplateBundle::GetConstructor(isolate, context));
}

}  // namespace

}  // namespace lynxtron::api

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_binding_lynx_template_bundle,
                                  lynxtron::api::Initialize)
