// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_APP_JAVASCRIPT_ENVIRONMENT_H_
#define LYNXTRON_SHELL_APP_JAVASCRIPT_ENVIRONMENT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "gin/public/isolate_holder.h"
#include "uv.h"  // NOLINT(build/include_directory)
#include "v8/include/v8-locker.h"

namespace node {
class Environment;
class MultiIsolatePlatform;
}  // namespace node

namespace lynxtron {

class MicrotasksRunner;
// Manage the V8 isolate and context automatically.
class JavascriptEnvironment {
 public:
  explicit JavascriptEnvironment(uv_loop_t* event_loop,
                                 bool setup_wasm_streaming = false);
  ~JavascriptEnvironment();

  // disable copy
  JavascriptEnvironment(const JavascriptEnvironment&) = delete;
  JavascriptEnvironment& operator=(const JavascriptEnvironment&) = delete;

  void CreateMicrotasksRunner();
  void DestroyMicrotasksRunner();

  node::MultiIsolatePlatform* platform() const { return platform_.get(); }
  v8::Isolate* isolate() const { return isolate_; }
  size_t max_young_generation_size_in_bytes() const {
    return max_young_generation_size_;
  }

  static v8::Isolate* GetIsolate();

 private:
  v8::Isolate* Initialize(uv_loop_t* event_loop, bool setup_wasm_streaming);
  std::unique_ptr<node::MultiIsolatePlatform> platform_;

  size_t max_young_generation_size_ = 0;
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;

  // owned-by: isolate_holder_
  const raw_ptr<v8::Isolate> isolate_;

  // depends-on: isolate_
  std::unique_ptr<v8::Locker> locker_;

  std::unique_ptr<MicrotasksRunner> microtasks_runner_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_JAVASCRIPT_ENVIRONMENT_H_
