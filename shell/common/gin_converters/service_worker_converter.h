// Copyright (c) 2025 Salesforce, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_GIN_CONVERTERS_SERVICE_WORKER_CONVERTER_H_
#define LYNXTRON_SHELL_COMMON_GIN_CONVERTERS_SERVICE_WORKER_CONVERTER_H_

#include "gin/converter.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"

namespace gin {

template <>
struct Converter<blink::EmbeddedWorkerStatus> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const blink::EmbeddedWorkerStatus& val);
};

}  // namespace gin

#endif  // LYNXTRON_SHELL_COMMON_GIN_CONVERTERS_SERVICE_WORKER_CONVERTER_H_
