// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_PROTOCOL_RESOURCE_HANDLER_H_
#define LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_PROTOCOL_RESOURCE_HANDLER_H_

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "node_buffer.h"
#include "platform/embedder/public/capi/lynx_generic_resource_fetcher_capi.h"
#include "platform/embedder/public/lynx_generic_resource_fetcher.h"
#include "shell/api/api_protocol.h"
#include "shell/common/gin_helper/dictionary.h"
#include "v8.h"

namespace lynxtron::resource_fetcher {

inline constexpr int kDefaultErrorCode = 1;

inline const char* GetResourceTypeString(lynx_resource_type_e type) {
  switch (type) {
    case kLynxResourceTypeGeneric:
      return "Generic";
    case kLynxResourceTypeImage:
      return "Image";
    case kLynxResourceTypeFont:
      return "Font";
    case kLynxResourceTypeLottie:
      return "Lottie";
    case kLynxResourceTypeVideo:
      return "Video";
    case kLynxResourceTypeSVG:
      return "SVG";
    case kLynxResourceTypeTemplate:
      return "Template";
    case kLynxResourceTypeLynxCoreJS:
      return "LynxCoreJS";
    case kLynxResourceTypeLazyBundle:
      return "LazyBundle";
    default:
      return "Unknown";
  }
}

inline void FreeVector(uint8_t*, size_t, void* opaque) {
  delete static_cast<std::vector<uint8_t>*>(opaque);
}

inline bool TrySetResponseDataFromV8Value(
    v8::Local<v8::Value> value,
    const std::shared_ptr<lynx::pub::resource::LynxResourceResponse>&
        response) {
  if (value.IsEmpty() || value->IsNullOrUndefined() ||
      !node::Buffer::HasInstance(value)) {
    return false;
  }

  const char* buf_data = node::Buffer::Data(value);
  const size_t buf_len = node::Buffer::Length(value);

  auto* vec = new std::vector<uint8_t>(buf_len);
  if (buf_len > 0) {
    std::memcpy(vec->data(), buf_data, buf_len);
  }
  response->SetData(vec->data(), buf_len, &FreeVector, vec);
  return true;
}

inline bool TrySetResponseFromResult(
    v8::Isolate* isolate,
    v8::Local<v8::Value> result,
    const std::shared_ptr<lynx::pub::resource::LynxResourceResponse>& response,
    int* status_code_out = nullptr) {
  if (result.IsEmpty() || result->IsFalse() || result->IsNullOrUndefined() ||
      !result->IsObject()) {
    return false;
  }

  gin_helper::Dictionary dict(isolate, result.As<v8::Object>());
  int status_code = kDefaultErrorCode;
  dict.Get("statusCode", &status_code);
  if (status_code_out) {
    *status_code_out = status_code;
  }

  v8::Local<v8::Value> data;
  if (!dict.Get("data", &data) ||
      !TrySetResponseDataFromV8Value(data, response)) {
    return false;
  }

  response->SetCode(status_code);
  response->Complete();
  return true;
}

inline std::string RewriteRequestUrl(
    v8::Isolate* isolate,
    const std::shared_ptr<lynx::pub::resource::LynxResourceRequest>& request,
    const std::string& url) {
  v8::Local<v8::Value> result;
  if (!api::protocol::InvokeRequestRewriter(
           isolate, url, GetResourceTypeString(request->GetType()))
           .ToLocal(&result) ||
      result.IsEmpty() || !result->IsString()) {
    return url;
  }

  v8::String::Utf8Value rewritten_url(isolate, result);
  if (*rewritten_url == nullptr) {
    return url;
  }
  return std::string(*rewritten_url, rewritten_url.length());
}

inline bool TryHandleProtocolRequest(
    v8::Isolate* isolate,
    const std::shared_ptr<lynx::pub::resource::LynxResourceRequest>& request,
    const std::shared_ptr<lynx::pub::resource::LynxResourceResponse>& response,
    const std::string& url) {
  v8::Local<v8::Value> result;
  if (!api::protocol::InvokeHandler(isolate, url,
                                    GetResourceTypeString(request->GetType()))
           .ToLocal(&result)) {
    return false;
  }
  return TrySetResponseFromResult(isolate, result, response);
}

}  // namespace lynxtron::resource_fetcher

#endif  // LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_PROTOCOL_RESOURCE_HANDLER_H_
