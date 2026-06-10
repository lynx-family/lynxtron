// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_RESOURCE_RESPONSE_UTILS_H_
#define LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_RESOURCE_RESPONSE_UTILS_H_

#include <memory>

#include "platform/embedder/public/lynx_generic_resource_fetcher.h"
#include "v8/include/v8-forward.h"

namespace lynxtron::resource_fetcher {

using LynxResourceResponse = lynx::pub::resource::LynxResourceResponse;

inline constexpr int kDefaultErrorCode = 1;

bool TrySetResponseFromResult(
    v8::Isolate* isolate,
    v8::Local<v8::Value> result,
    const std::shared_ptr<LynxResourceResponse>& response,
    int* status_code_out = nullptr);

}  // namespace lynxtron::resource_fetcher

#endif  // LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_RESOURCE_RESPONSE_UTILS_H_
