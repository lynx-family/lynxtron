// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_LYNX_HTTP_SERVICE_LYNX_HTTP_SERVICE_BINDING_H_
#define LYNXTRON_SHELL_LYNX_HTTP_SERVICE_LYNX_HTTP_SERVICE_BINDING_H_

#include "v8/include/v8-forward.h"

namespace lynxtron {

bool InvokeLynxHttpRequestHandler(v8::Isolate* isolate,
                                  v8::Local<v8::Value> request,
                                  v8::Local<v8::Function> reply);

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_LYNX_HTTP_SERVICE_LYNX_HTTP_SERVICE_BINDING_H_
