// Copyright (c) 2019 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_COMMON_GIN_CONVERTERS_NATIVE_WINDOW_CONVERTER_H_
#define LYNXTRON_SHELL_COMMON_GIN_CONVERTERS_NATIVE_WINDOW_CONVERTER_H_

#include "gin/converter.h"
#include "shell/api/api_base_window.h"

namespace gin {

template <>
struct Converter<lynxtron::NativeWindow*> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     lynxtron::NativeWindow** out) {
    // null would be transferred to nullptr.
    if (val->IsNull()) {
      *out = nullptr;
      return true;
    }

    lynxtron::api::BaseWindow* window;
    if (!gin::Converter<lynxtron::api::BaseWindow*>::FromV8(isolate, val,
                                                            &window)) {
      return false;
    }
    *out = window->window();
    return true;
  }
};

}  // namespace gin

#endif  // LYNXTRON_SHELL_COMMON_GIN_CONVERTERS_NATIVE_WINDOW_CONVERTER_H_
