// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_LYNX_TRACE_RUNTIME_PROFILE_HELPER_H_
#define LYNXTRON_SHELL_LYNX_TRACE_RUNTIME_PROFILE_HELPER_H_

#if ENABLE_TRACE_PERFETTO

#include <memory>

#include "lynx/core/runtime/profile/v8/v8_runtime_profiler.h"
#include "v8.h"

namespace lynxtron {
namespace trace {

class RuntimeProfileHelper {
 public:
  RuntimeProfileHelper() = default;
  ~RuntimeProfileHelper() = default;

  static RuntimeProfileHelper& GetInstance();

  void SetV8RuntimeProfiler(v8::Isolate* isolate);
  void RemoveV8RuntimeProfiler();

 private:
  std::shared_ptr<lynx::runtime::profile::V8RuntimeProfiler> runtime_profiler_ =
      nullptr;
};

}  // namespace trace
}  // namespace lynxtron

#endif  // ENABLE_TRACE_PERFETTO

#endif  // LYNXTRON_SHELL_LYNX_TRACE_RUNTIME_PROFILE_HELPER_H_
