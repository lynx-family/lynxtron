// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_MODULE_LYNX_MODULE_UTILS_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_MODULE_LYNX_MODULE_UTILS_H_

#include <vector>

#include "base/containers/span.h"

namespace v8 {
class Isolate;
class Context;
class Value;
template <typename T>
class Local;
}  // namespace v8

namespace lynxtron {

using V8SerializerValue = std::vector<uint8_t>;

V8SerializerValue SerializeValue(v8::Isolate* isolate,
                                 v8::Local<v8::Context> context,
                                 v8::Local<v8::Value> value);

v8::Local<v8::Value> DeserializeValue(v8::Isolate* isolate,
                                      v8::Local<v8::Context> context,
                                      base::span<const uint8_t> value);

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_MODULE_LYNX_MODULE_UTILS_H_
