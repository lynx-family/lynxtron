// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/gin_converters/accelerator_converter.h"

#include <string>

#include "shell/ui/accelerator_util.h"

namespace gin {

// static
bool Converter<ui::Accelerator>::FromV8(v8::Isolate* isolate,
                                        v8::Local<v8::Value> val,
                                        ui::Accelerator* out) {
  std::string keycode;
  if (!ConvertFromV8(isolate, val, &keycode)) {
    return false;
  }
  return accelerator_util::StringToAccelerator(keycode, out);
}

}  // namespace gin
