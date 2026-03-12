// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if ENABLE_TRACE_PERFETTO

#include "lynx/trace/runtime_profile_helper.h"

#include "lynx/core/runtime/js/jsi/v8/v8_isolate_wrapper.h"
#include "lynx/core/runtime/profile/runtime_profiler_manager.h"
#include "lynx/core/runtime/profile/v8/v8_runtime_profiler_wrapper_impl.h"

namespace lynxtron {
namespace trace {

namespace {
class LynxtronV8IsolateInstance : public lynx::runtime::js::V8IsolateInstance {
 public:
  explicit LynxtronV8IsolateInstance(v8::Isolate* isolate)
      : isolate_(isolate){};
  ~LynxtronV8IsolateInstance(){};

  void InitIsolate(const char* arg, bool useSnapshot) override{};

  v8::Isolate* Isolate() const override { return isolate_; };

 private:
  v8::Isolate* isolate_;
};
}  // namespace

std::shared_ptr<lynx::runtime::profile::V8RuntimeProfiler>
    RuntimeProfileHelper::runtime_profiler_ = nullptr;

void RuntimeProfileHelper::SetV8RuntimeProfiler(v8::Isolate* isolate) {
  auto v8_profiler =
      lynx::runtime::profile::V8RuntimeProfilerWrapperImpl::GetInstance();
  auto v8_isolate_instance =
      std::make_shared<LynxtronV8IsolateInstance>(isolate);
  v8_profiler->Initialize(v8_isolate_instance);
  runtime_profiler_ =
      std::make_shared<lynx::runtime::profile::V8RuntimeProfiler>(v8_profiler,
                                                                  true);
  lynx::runtime::profile::RuntimeProfilerManager::GetInstance()
      ->AddRuntimeProfiler(runtime_profiler_);
}
void RuntimeProfileHelper::RemoveV8RuntimeProfiler() {
  lynx::runtime::profile::RuntimeProfilerManager::GetInstance()
      ->RemoveRuntimeProfiler(runtime_profiler_);
  runtime_profiler_.reset();
}

}  // namespace trace
}  // namespace lynxtron

#endif  // ENABLE_TRACE_PERFETTO
