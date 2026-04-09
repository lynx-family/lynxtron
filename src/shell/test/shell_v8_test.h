// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_TEST_SHELL_V8_TEST_H_
#define LYNXTRON_SHELL_TEST_SHELL_V8_TEST_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-local-handle.h"

namespace lynxtron {

class ShellV8Test : public gin::V8Test {
 protected:
  std::unique_ptr<gin::IsolateHolder> CreateIsolateHolder() const override {
    return std::make_unique<gin::IsolateHolder>(
        base::SingleThreadTaskRunner::GetCurrentDefault(), AccessMode(),
        gin::IsolateHolder::IsolateType::kTest);
  }

  v8::Isolate* isolate() const { return instance_->isolate(); }

  v8::Local<v8::Context> GetContext() {
    return v8::Local<v8::Context>::New(isolate(), context_);
  }
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_TEST_SHELL_V8_TEST_H_
