// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/lynx/resource_fetcher/lynx_resource_response_utils.h"

#include <cstring>
#include <vector>

#include "node_buffer.h"
#include "shell/common/gin_helper/dictionary.h"
#include "v8.h"

namespace lynxtron::resource_fetcher {
namespace {

void FreeVector(uint8_t*, size_t, void* opaque) {
  delete static_cast<std::vector<uint8_t>*>(opaque);
}

bool TrySetResponseDataFromV8Value(
    v8::Local<v8::Value> value,
    const std::shared_ptr<LynxResourceResponse>& response) {
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

}  // namespace

bool TrySetResponseFromResult(
    v8::Isolate* isolate,
    v8::Local<v8::Value> result,
    const std::shared_ptr<LynxResourceResponse>& response,
    int* status_code_out) {
  if (result.IsEmpty() || result->IsFalse() || result->IsNullOrUndefined() ||
      !result->IsObject()) {
    return false;
  }

  gin_helper::Dictionary dict(isolate, result.As<v8::Object>());
  int status_code = kDefaultErrorCode;
  if (!dict.Get("statusCode", &status_code)) {
    return false;
  }
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

}  // namespace lynxtron::resource_fetcher
