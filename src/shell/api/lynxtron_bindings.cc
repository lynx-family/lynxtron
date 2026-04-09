// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynxtron_bindings.h"

#include "base/containers/contains.h"
#include "base/process/process.h"
#include "base/system/sys_info.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/locker.h"
#include "shell/common/node_includes.h"

#if BUILDFLAG(IS_WIN)
#include "shell/common/application_info.h"
#endif

namespace lynxtron {

LynxtronBindings::LynxtronBindings(uv_loop_t* loop) {
  uv_async_init(loop, call_next_tick_async_.get(), OnCallNextTick);
  call_next_tick_async_.get()->data = this;
  metrics_ = base::ProcessMetrics::CreateCurrentProcessMetrics();
}

LynxtronBindings::~LynxtronBindings() = default;

// static
void LynxtronBindings::BindProcess(v8::Isolate* isolate,
                                   gin_helper::Dictionary* process,
                                   base::ProcessMetrics* metrics) {
  // These bindings are shared between sandboxed & unsandboxed renderers
  process->SetMethod("crash", &Crash);
  process->SetMethod("hang", &Hang);
  process->SetMethod("getCreationTime", &GetCreationTime);
  process->SetMethod("getHeapStatistics", &GetHeapStatistics);
  process->SetMethod("getSystemVersion",
                     &base::SysInfo::OperatingSystemVersion);
  process->SetMethod("getCPUUsage",
                     base::BindRepeating(&LynxtronBindings::GetCPUUsage,
                                         base::Unretained(metrics)));

#if BUILDFLAG(IS_WIN)
  if (IsRunningInDesktopBridge()) {
    process->SetReadOnly("windowsStore", true);
  }
#endif
}

void LynxtronBindings::BindTo(v8::Isolate* isolate,
                              v8::Local<v8::Object> process) {
  gin_helper::Dictionary dict(isolate, process);
  BindProcess(isolate, &dict, metrics_.get());

#if BUILDFLAG(IS_POSIX)
  dict.SetMethod("setFdLimit", &base::IncreaseFdLimitTo);
#endif
  dict.SetMethod("activateUvLoop",
                 base::BindRepeating(&LynxtronBindings::ActivateUVLoop,
                                     base::Unretained(this)));
}

void LynxtronBindings::EnvironmentDestroyed(node::Environment* env) {
  std::erase(pending_next_ticks_, env);
}

void LynxtronBindings::ActivateUVLoop(v8::Isolate* isolate) {
  node::Environment* env = node::Environment::GetCurrent(isolate);
  if (base::Contains(pending_next_ticks_, env)) {
    return;
  }

  pending_next_ticks_.push_back(env);
  uv_async_send(call_next_tick_async_.get());
}

// static
void LynxtronBindings::OnCallNextTick(uv_async_t* handle) {
  auto* self = static_cast<LynxtronBindings*>(handle->data);
  for (auto* env : self->pending_next_ticks_) {
    gin_helper::Locker locker(env->isolate());
    v8::Context::Scope context_scope(env->context());
    v8::HandleScope handle_scope(env->isolate());
    node::CallbackScope scope(env->isolate(), v8::Object::New(env->isolate()),
                              {0, 0});
  }

  self->pending_next_ticks_.clear();
}

// static
void LynxtronBindings::Crash() {
  volatile int* zero = nullptr;
  *zero = 0;
}

// static
void LynxtronBindings::Hang() {
  for (;;) {
    base::PlatformThread::Sleep(base::Seconds(1));
  }
}

// static
v8::Local<v8::Value> LynxtronBindings::GetHeapStatistics(v8::Isolate* isolate) {
  v8::HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);

  auto dict = gin_helper::Dictionary::CreateEmpty(isolate);
  dict.Set("totalHeapSize",
           static_cast<double>(v8_heap_stats.total_heap_size() >> 10));
  dict.Set(
      "totalHeapSizeExecutable",
      static_cast<double>(v8_heap_stats.total_heap_size_executable() >> 10));
  dict.Set("totalPhysicalSize",
           static_cast<double>(v8_heap_stats.total_physical_size() >> 10));
  dict.Set("totalAvailableSize",
           static_cast<double>(v8_heap_stats.total_available_size() >> 10));
  dict.Set("usedHeapSize",
           static_cast<double>(v8_heap_stats.used_heap_size() >> 10));
  dict.Set("heapSizeLimit",
           static_cast<double>(v8_heap_stats.heap_size_limit() >> 10));
  dict.Set("mallocedMemory",
           static_cast<double>(v8_heap_stats.malloced_memory() >> 10));
  dict.Set("peakMallocedMemory",
           static_cast<double>(v8_heap_stats.peak_malloced_memory() >> 10));
  dict.Set("doesZapGarbage",
           static_cast<bool>(v8_heap_stats.does_zap_garbage()));

  return dict.GetHandle();
}

// static
v8::Local<v8::Value> LynxtronBindings::GetCreationTime(v8::Isolate* isolate) {
  auto timeValue = base::Process::Current().CreationTime();
  if (timeValue.is_null()) {
    return v8::Null(isolate);
  }
  double jsTime = timeValue.InMillisecondsFSinceUnixEpoch();
  return v8::Number::New(isolate, jsTime);
}

// static
v8::Local<v8::Value> LynxtronBindings::GetCPUUsage(
    base::ProcessMetrics* metrics,
    v8::Isolate* isolate) {
  auto dict = gin_helper::Dictionary::CreateEmpty(isolate);
  int processor_count = base::SysInfo::NumberOfProcessors();

  // Default usage percentage to 0 for compatibility
  double usagePercent = 0;
  if (auto usage = metrics->GetCumulativeCPUUsage(); usage.has_value()) {
    dict.Set("cumulativeCPUUsage", usage->InSecondsF());
    usagePercent = metrics->GetPlatformIndependentCPUUsage(*usage);
  }

  dict.Set("percentCPUUsage", usagePercent / processor_count);

  // NB: This will throw NOTIMPLEMENTED() on Windows
  // For backwards compatibility, we'll return 0
#if !BUILDFLAG(IS_WIN)
  dict.Set("idleWakeupsPerSecond", metrics->GetIdleWakeupsPerSecond());
#else
  dict.Set("idleWakeupsPerSecond", 0);
#endif

  return dict.GetHandle();
}

}  // namespace lynxtron
