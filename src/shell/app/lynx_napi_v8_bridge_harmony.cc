// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// HarmonyOS shims:
//   1. base::debug::StackTrace::OutputToStreamWithPrefixImpl (chromium base has
//      no HAVE_BACKTRACE on OHOS).
//   2. PrimJS-NAPI per-engine attach/detach for the engines Lynxtron does NOT
//      use on harmony (QuickJS/lepus, HarmonyOS JSVM). The V8 engine bridge
//      (napi_attach_v8 / napi_get_env_context_v8 / napi_*_value_to_*_value) now
//      comes from the REAL //lynx/third_party/napi:v8 (js_native_api_v8.cc), so
//      the primjs napi env's method table is actually wired to V8 — the "V8 +
//      NS" seam. Without it, napi_attach_v8 was a no-op and the env's
//      get_instance_data etc. stayed null, crashing in
//      TemplateEntry::ConstructContext -> CallbackHelper::SetUncaughtExceptionHandler
//      -> Napi::Env::GetInstanceData.

#include <ostream>

#include "base/debug/stack_trace.h"
#include "base/strings/cstring_view.h"

// Only compiled in base/debug/stack_trace_posix.cc under #if HAVE_BACKTRACE;
// HarmonyOS has no execinfo backtrace, so provide a no-op.
namespace base {
namespace debug {
void StackTrace::OutputToStreamWithPrefixImpl(
    std::ostream* os, base::cstring_view prefix_string) const {
  if (os) {
    *os << prefix_string << "[stack trace unavailable on harmony]\n";
  }
}
}  // namespace debug
}  // namespace base

// PrimJS-NAPI attach/detach for the HarmonyOS JSVM engine — never used by
// Lynxtron (BTS runs on Node V8 via //lynx/third_party/napi:v8, the main/tasm
// lepus context on QuickJS via //lynx/third_party/napi:quickjs), but referenced
// by the multi-engine dispatch, so keep no-op stubs. extern "C" to match the
// primjs harmony napi header's linkage.
extern "C" {
void napi_attach_harmony(void*, void*) {}
void napi_detach_harmony(void*) {}
}  // extern "C"
