// Copyright (c) 2020 Slack Technologies, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_V8_UTIL_H_
#define LYNXTRON_SHELL_COMMON_V8_UTIL_H_

#include <vector>

#include "base/containers/span.h"
// #include "ui/gfx/image/image_skia_rep.h"

namespace v8 {
class ArrayBufferView;
class Isolate;
template <class T>
class Local;
class Value;
}  // namespace v8

// namespace blink {
// struct CloneableMessage;
// }

namespace lynxtron {

bool SerializeV8Value(v8::Isolate* isolate,
                      v8::Local<v8::Value> value,
                      std::vector<uint8_t>& out);

v8::Local<v8::Value> DeserializeV8Value(v8::Isolate* isolate,
                                        base::span<const uint8_t> data);

namespace util {

[[nodiscard]] base::span<uint8_t> as_byte_span(
    v8::Local<v8::ArrayBufferView> abv);

}  // namespace util
}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_V8_UTIL_H_
