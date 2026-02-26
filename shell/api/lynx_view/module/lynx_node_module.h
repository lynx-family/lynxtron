// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_LYNX_VIEW_HOLDER_MODULE_LYNX_NODE_MODULE_H_
#define LYNXTRON_SHELL_LYNX_VIEW_HOLDER_MODULE_LYNX_NODE_MODULE_H_

#include <string>
#include <vector>

#include "lynx/platform/embedder/public/capi/lynx_view_builder_capi.h"

#ifdef USE_PRIMJS_NAPI
#include "third_party/napi/include/primjs_napi_defines.h"
#endif

namespace lynxtron {

void SetNodePlatformEnvToLynxNodeModule(void* v8_platform);
void RegisterLynxNodeModuleToLynxView(
    lynx_view_builder_t* builder,
    const std::vector<std::string>& node_integration_preload);

}  // namespace lynxtron

#ifdef USE_PRIMJS_NAPI
#include "third_party/napi/include/primjs_napi_undefs.h"
#endif

#endif  // LYNXTRON_SHELL_LYNX_VIEW_HOLDER_MODULE_LYNX_NODE_MODULE_H_
