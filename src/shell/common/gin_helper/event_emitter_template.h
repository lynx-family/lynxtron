// Copyright (c) 2023 Salesforce, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_GIN_HELPER_EVENT_EMITTER_TEMPLATE_H_
#define LYNXTRON_SHELL_COMMON_GIN_HELPER_EVENT_EMITTER_TEMPLATE_H_

namespace v8 {
class Isolate;
template <typename T>
class Local;
class FunctionTemplate;
}  // namespace v8

namespace gin_helper::internal {
v8::Local<v8::FunctionTemplate> GetEventEmitterTemplate(v8::Isolate* isolate);
}

#endif  // LYNXTRON_SHELL_COMMON_GIN_HELPER_EVENT_EMITTER_TEMPLATE_H_
