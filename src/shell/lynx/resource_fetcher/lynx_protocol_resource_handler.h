// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_PROTOCOL_RESOURCE_HANDLER_H_
#define LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_PROTOCOL_RESOURCE_HANDLER_H_

#include <string>

namespace lynxtron::resource_fetcher {

std::string RewriteRequestUrl(const std::string& url);

}  // namespace lynxtron::resource_fetcher

#endif  // LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_PROTOCOL_RESOURCE_HANDLER_H_
