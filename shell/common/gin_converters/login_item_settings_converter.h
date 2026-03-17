// Copyright (c) 2024 Microsoft, GmbH.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_GIN_CONVERTERS_LOGIN_ITEM_SETTINGS_CONVERTER_H_
#define LYNXTRON_SHELL_COMMON_GIN_CONVERTERS_LOGIN_ITEM_SETTINGS_CONVERTER_H_

#include "gin/converter.h"

namespace lynxtron {
struct LoginItemSettings;
struct LaunchItem;
}  // namespace lynxtron

namespace gin {

#if BUILDFLAG(IS_WIN)
template <>
struct Converter<lynxtron::LaunchItem> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   lynxtron::LaunchItem val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     lynxtron::LaunchItem* out);
};
#endif

template <>
struct Converter<lynxtron::LoginItemSettings> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   lynxtron::LoginItemSettings val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     lynxtron::LoginItemSettings* out);
};

}  // namespace gin

#endif  // LYNXTRON_SHELL_COMMON_GIN_CONVERTERS_LOGIN_ITEM_SETTINGS_CONVERTER_H_
