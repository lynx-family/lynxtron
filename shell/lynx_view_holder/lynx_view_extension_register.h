// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_LYNX_VIEW_HOLDER_LYNX_VIEW_EXTENSION_REGISTER_H_
#define LYNXTRON_SHELL_LYNX_VIEW_HOLDER_LYNX_VIEW_EXTENSION_REGISTER_H_

namespace lynxtron {

void RegisterLynxExtensionToGlobalEnv(const char* name,
                                      void* creator_fun,
                                      bool is_lazy_create,
                                      void* opaque);

}

#endif
