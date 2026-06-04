// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/api/lynx_view/lynx_view_extension_register.h"

#include <cstring>

#include "base/logging.h"
#include "build/build_config.h"
#include "lynx/platform/embedder/public/capi/lynx_env_capi.h"
#include "lynx/platform/embedder/public/capi/lynx_extension_module_types_capi.h"

typedef struct lynx_extension_module_creator_api_t {
  extension_module_creator create_module_func;
} lynx_extension_module_creator_api_t;

namespace lynxtron {
void RegisterLynxExtensionToGlobalEnv(const char* name,
                                      void* creator_api,
                                      bool is_lazy_create,
                                      void* opaque) {
  if (!name || strlen(name) == 0) {
    LOG(ERROR)
        << "RegisterLynxExtensionToGlobalEnv failed. name is null or empty";
    return;
  }

  if (!creator_api) {
    LOG(ERROR) << "RegisterLynxExtensionToGlobalEnv failed. create_module_func "
                  "is null";
    return;
  }

  lynx_env_register_extension_module(
      name,
      static_cast<lynx_extension_module_creator_api_t*>(creator_api)
          ->create_module_func,
      is_lazy_create, opaque);
}
}  // namespace lynxtron
