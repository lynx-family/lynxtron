
// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_MODULE_LYNX_HYBRID_MONITOR_MODULE_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_MODULE_LYNX_HYBRID_MONITOR_MODULE_H_

#if defined(__cplusplus)

#include "base/memory/weak_ptr.h"
#include "lynx/platform/embedder/public/capi/lynx_view_builder_capi.h"

namespace lynxtron {
namespace api {
class LynxWindow;
}  // namespace api

void RegisterLynxHybridMonitorModuleToLynxView(
    lynx_view_builder_t* builder,
    base::WeakPtr<api::LynxWindow> lynx_window);

}  // namespace lynxtron

#endif

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_MODULE_LYNX_HYBRID_MONITOR_MODULE_H_
