// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_protocol.h"

#include <cctype>
#include <map>
#include <string>

#include "base/no_destructor.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/error_thrower.h"
#include "shell/common/node_includes.h"
#include "v8.h"

namespace lynxtron::api::protocol {
namespace {

using ProtocolHandlerMap = std::map<std::string, v8::Global<v8::Function>>;

ProtocolHandlerMap& GetProtocolHandlers() {
  static base::NoDestructor<ProtocolHandlerMap> protocol_handlers;
  return *protocol_handlers;
}

v8::Global<v8::Function>& GetRequestRewriter() {
  static base::NoDestructor<v8::Global<v8::Function>> request_rewriter;
  return *request_rewriter;
}

std::string NormalizeScheme(std::string scheme) {
  if (!scheme.empty() && scheme.back() == ':') {
    scheme.pop_back();
  }
  for (char& c : scheme) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return scheme;
}

bool IsValidScheme(const std::string& scheme) {
  if (scheme.empty() || !std::isalpha(static_cast<unsigned char>(scheme[0]))) {
    return false;
  }
  for (char c : scheme) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '-' &&
        c != '.') {
      return false;
    }
  }
  return true;
}

std::string ExtractSchemeFromURL(const std::string& url) {
  const size_t separator = url.find(':');
  if (separator == std::string::npos || separator == 0) {
    return std::string();
  }
  return NormalizeScheme(url.substr(0, separator));
}

v8::MaybeLocal<v8::Object> CreateResourceRequest(
    v8::Isolate* isolate,
    const std::string& url,
    const std::string& scheme,
    const std::string& resource_type) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> request = v8::Object::New(isolate);
  const auto set_string = [&](const char* key, const std::string& value) {
    return request
        ->Set(context, v8::String::NewFromUtf8(isolate, key).ToLocalChecked(),
              v8::String::NewFromUtf8(isolate, value.c_str()).ToLocalChecked())
        .FromMaybe(false);
  };
  if (!set_string("url", url) || !set_string("scheme", scheme) ||
      !set_string("resourceType", resource_type)) {
    return v8::MaybeLocal<v8::Object>();
  }
  return request;
}

v8::MaybeLocal<v8::Value> InvokeCallback(v8::Isolate* isolate,
                                         v8::Local<v8::Function> callback,
                                         const std::string& url,
                                         const std::string& scheme,
                                         const std::string& resource_type) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> request;
  if (!CreateResourceRequest(isolate, url, scheme, resource_type)
           .ToLocal(&request)) {
    return v8::MaybeLocal<v8::Value>();
  }

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> argv[] = {request};
  v8::MaybeLocal<v8::Value> maybe_result =
      callback->Call(context, v8::Undefined(isolate), 1, argv);
  if (try_catch.HasCaught() || maybe_result.IsEmpty()) {
    return v8::MaybeLocal<v8::Value>();
  }
  return maybe_result;
}

}  // namespace

void Handle(gin_helper::ErrorThrower thrower,
            const std::string& scheme,
            v8::Local<v8::Value> handler) {
  const std::string normalized_scheme = NormalizeScheme(scheme);
  if (!IsValidScheme(normalized_scheme)) {
    thrower.ThrowTypeError("Invalid protocol scheme");
    return;
  }
  if (!handler->IsFunction()) {
    thrower.ThrowTypeError("handler must be a function");
    return;
  }

  GetProtocolHandlers()[normalized_scheme].Reset(thrower.isolate(),
                                                 handler.As<v8::Function>());
}

void Unhandle(gin_helper::ErrorThrower thrower, const std::string& scheme) {
  const std::string normalized_scheme = NormalizeScheme(scheme);
  if (!IsValidScheme(normalized_scheme)) {
    thrower.ThrowTypeError("Invalid protocol scheme");
    return;
  }

  auto& protocol_handlers = GetProtocolHandlers();
  auto it = protocol_handlers.find(normalized_scheme);
  if (it == protocol_handlers.end()) {
    return;
  }
  it->second.Reset();
  protocol_handlers.erase(it);
}

bool IsProtocolHandled(const std::string& scheme) {
  const std::string normalized_scheme = NormalizeScheme(scheme);
  if (!IsValidScheme(normalized_scheme)) {
    return false;
  }
  return GetProtocolHandlers().contains(normalized_scheme);
}

void SetRequestRewriter(gin_helper::ErrorThrower thrower,
                        v8::Local<v8::Value> handler) {
  auto& request_rewriter = GetRequestRewriter();
  if (handler->IsNullOrUndefined()) {
    request_rewriter.Reset();
    return;
  }
  if (!handler->IsFunction()) {
    thrower.ThrowTypeError("handler must be a function");
    return;
  }

  request_rewriter.Reset(thrower.isolate(), handler.As<v8::Function>());
}

v8::MaybeLocal<v8::Value> InvokeHandler(v8::Isolate* isolate,
                                        const std::string& url,
                                        const std::string& resource_type) {
  const std::string scheme = ExtractSchemeFromURL(url);
  if (scheme.empty()) {
    return v8::False(isolate);
  }

  auto& protocol_handlers = GetProtocolHandlers();
  auto it = protocol_handlers.find(scheme);
  if (it == protocol_handlers.end()) {
    return v8::False(isolate);
  }

  return InvokeCallback(isolate, it->second.Get(isolate), url, scheme,
                        resource_type);
}

v8::MaybeLocal<v8::Value> InvokeRequestRewriter(
    v8::Isolate* isolate,
    const std::string& url,
    const std::string& resource_type) {
  auto& request_rewriter = GetRequestRewriter();
  if (request_rewriter.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  return InvokeCallback(isolate, request_rewriter.Get(isolate), url,
                        ExtractSchemeFromURL(url), resource_type);
}

}  // namespace lynxtron::api::protocol

namespace {

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* const isolate = v8::Isolate::GetCurrent();
  gin_helper::Dictionary dict{isolate, exports};
  dict.SetMethod("handle", &lynxtron::api::protocol::Handle);
  dict.SetMethod("unhandle", &lynxtron::api::protocol::Unhandle);
  dict.SetMethod("isProtocolHandled",
                 &lynxtron::api::protocol::IsProtocolHandled);
  dict.SetMethod("setRequestRewriter",
                 &lynxtron::api::protocol::SetRequestRewriter);
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_binding_protocol, Initialize)
