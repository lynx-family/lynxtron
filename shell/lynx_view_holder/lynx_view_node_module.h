// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_LYNX_VIEW_HOLDER_LYNX_VIEW_NODE_MODULE_H_
#define LYNXTRON_SHELL_LYNX_VIEW_HOLDER_LYNX_VIEW_NODE_MODULE_H_

#include "lynx/platform/embedder/public/capi/lynx_view_builder_capi.h"

namespace lynxtron {

void SetNodePlatformEnvToLynxNodeModule(void* v8_platform);
void RegisterLynxNodeModuleToLynxView(lynx_view_builder_t* builder);
void RegisterLynxNodeModuleGlobal();

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_LYNX_VIEW_HOLDER_LYNX_VIEW_NODE_MODULE_H_
