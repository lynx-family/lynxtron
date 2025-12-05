// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef PLUGIN_CEF_CEF_EXTENSION_MODULE_H_
#define PLUGIN_CEF_CEF_EXTENSION_MODULE_H_

#include "lynx/platform/embedder/public/capi/lynx_export.h"
#include "lynx/platform/embedder/public/lynx_extension_module.h"
namespace lynxtron {
namespace plugin {

class CEFExtensionModule : public lynx::pub::LynxExtensionModule {
 public:
  void OnLynxViewCreate(lynx_view_t* lynx_view) override;
};

}  // namespace plugin
}  // namespace lynxtron

LYNX_EXTERN_C_BEGIN
LYNX_CAPI_EXPORT lynx_extension_module_t*
cef_extension_module_create_extension_module(void* opaque);
LYNX_EXTERN_C_END

#endif  // PLUGIN_CEF_CEF_EXTENSION_MODULE_H_
