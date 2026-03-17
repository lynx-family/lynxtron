// Copyright (c) 2019 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_GIN_CONVERTERS_FILE_DIALOG_CONVERTER_H_
#define LYNXTRON_SHELL_COMMON_GIN_CONVERTERS_FILE_DIALOG_CONVERTER_H_

#include "gin/converter.h"
#include "shell/api/ui/file_dialog.h"

namespace gin {

template <>
struct Converter<file_dialog::Filter> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const file_dialog::Filter& in);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     file_dialog::Filter* out);
};

template <>
struct Converter<file_dialog::DialogSettings> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const file_dialog::DialogSettings& in);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     file_dialog::DialogSettings* out);
};

}  // namespace gin

#endif  // LYNXTRON_SHELL_COMMON_GIN_CONVERTERS_FILE_DIALOG_CONVERTER_H_
