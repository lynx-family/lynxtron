// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_FILE_RESOURCE_LOADER_H_
#define LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_FILE_RESOURCE_LOADER_H_

#include <memory>
#include <string>

#include "platform/embedder/public/lynx_resource_response.h"

namespace lynxtron::resource_fetcher {

// Loads local files and ASAR members; returns false for other URL schemes.
bool TryLoadFileResource(
    const std::string& url,
    const std::shared_ptr<lynx::pub::resource::LynxResourceResponse>& response);

}  // namespace lynxtron::resource_fetcher

#endif  // LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_FILE_RESOURCE_LOADER_H_
