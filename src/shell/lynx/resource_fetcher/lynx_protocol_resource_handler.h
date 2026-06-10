// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_PROTOCOL_RESOURCE_HANDLER_H_
#define LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_PROTOCOL_RESOURCE_HANDLER_H_

#include <functional>
#include <memory>
#include <string>

#include "platform/embedder/public/capi/lynx_generic_resource_fetcher_capi.h"
#include "platform/embedder/public/lynx_generic_resource_fetcher.h"

namespace lynxtron::resource_fetcher {

using LynxResourceRequest = lynx::pub::resource::LynxResourceRequest;
using LynxResourceResponse = lynx::pub::resource::LynxResourceResponse;

const char* GetResourceTypeString(lynx_resource_type_e type);

std::string RewriteRequestUrl(const std::string& url);

void HandleProtocolRequest(
    const std::shared_ptr<LynxResourceRequest>& request,
    const std::shared_ptr<LynxResourceResponse>& response,
    const std::string& url,
    std::function<void()> fallback);

}  // namespace lynxtron::resource_fetcher

#endif  // LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_PROTOCOL_RESOURCE_HANDLER_H_
