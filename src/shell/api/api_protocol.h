// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_API_PROTOCOL_H_
#define LYNXTRON_SHELL_API_API_PROTOCOL_H_

#include <string>

#include "v8/include/v8-forward.h"

namespace lynxtron::api::protocol {

v8::MaybeLocal<v8::Value> InvokeHandler(v8::Isolate* isolate,
                                        const std::string& url,
                                        const std::string& resource_type);

v8::MaybeLocal<v8::Value> InvokeRequestRewriter(v8::Isolate* isolate,
                                                const std::string& url);

}  // namespace lynxtron::api::protocol

#endif  // LYNXTRON_SHELL_API_API_PROTOCOL_H_
