// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNXTRON_BINDINGS_H_
#define LYNXTRON_SHELL_API_LYNXTRON_BINDINGS_H_

#include <list>
#include <memory>

#include "base/process/process_metrics.h"
#include "shell/common/node_bindings.h"
#include "uv.h"  // NOLINT(build/include_directory)

namespace base {
class FilePath;
}

namespace gin_helper {
class Arguments;
class Dictionary;
template <typename T>
class Promise;
}  // namespace gin_helper

namespace memory_instrumentation {
class GlobalMemoryDump;
}

namespace node {
class Environment;
}

namespace lynxtron {

class LynxtronBindings {
 public:
  explicit LynxtronBindings(uv_loop_t* loop);
  virtual ~LynxtronBindings();

  // disable copy
  LynxtronBindings(const LynxtronBindings&) = delete;
  LynxtronBindings& operator=(const LynxtronBindings&) = delete;

  // Add process._linkedBinding function, which behaves like process.binding
  // but load native code from Electron instead.
  void BindTo(v8::Isolate* isolate, v8::Local<v8::Object> process);

  // Should be called when a node::Environment has been destroyed.
  void EnvironmentDestroyed(node::Environment* env);

  static void BindProcess(v8::Isolate* isolate,
                          gin_helper::Dictionary* process,
                          base::ProcessMetrics* metrics);

  static void Crash();

  static void DidReceiveMemoryDump(
      v8::Global<v8::Context> context,
      gin_helper::Promise<gin_helper::Dictionary> promise,
      base::ProcessId target_pid,
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);

 private:
  static void Hang();
  static v8::Local<v8::Value> GetHeapStatistics(v8::Isolate* isolate);
  static v8::Local<v8::Value> GetCreationTime(v8::Isolate* isolate);
  static v8::Local<v8::Value> GetSystemMemoryInfo(v8::Isolate* isolate,
                                                  gin_helper::Arguments* args);
  static v8::Local<v8::Value> GetCPUUsage(base::ProcessMetrics* metrics,
                                          v8::Isolate* isolate);

  void ActivateUVLoop(v8::Isolate* isolate);

  static void OnCallNextTick(uv_async_t* handle);

  UvHandle<uv_async_t> call_next_tick_async_;
  std::list<node::Environment*> pending_next_ticks_;
  std::unique_ptr<base::ProcessMetrics> metrics_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_LYNXTRON_BINDINGS_H_
