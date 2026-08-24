// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Native bindings for lynxtron.checkAppUpdate(), lynxtron.showUpdateDialog(),
// and lynxtron.loadProduct().
//
// Each function returns a JS Promise. On HarmonyOS, the request is pushed to
// ArkTS via napi_threadsafe_function (TSFN) — the NAPI bridge registers a
// TSFN at init time, and we call napi_call_threadsafe_function() to wake the
// ArkTS callback instantly instead of polling.
//
// On non-HarmonyOS, Promises are rejected immediately.

#include "shell/api/api_update_check.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <mutex>
#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/global_thread.h"
#include "shell/common/node_includes.h"
#include "third_party/napi/include/js_native_api.h"
#include "third_party/napi/include/js_native_api_types.h"

#define ZYBAPI_TAG "[UpdateModel api_update_check.cc]"
#define ZYBAPI_LOG(fmt, ...)                                  \
  do {                                                        \
    auto _now = std::chrono::system_clock::now();              \
    auto _ms = std::chrono::duration_cast<std::chrono::milliseconds>( \
                   _now.time_since_epoch()).count() % 1000;   \
    std::time_t _tt = std::chrono::system_clock::to_time_t(_now); \
    struct tm _tm;                                            \
    localtime_r(&_tt, &_tm);                                  \
    fprintf(stderr, "%02d%02d%02d.%03lld " ZYBAPI_TAG fmt "\n", \
            _tm.tm_hour, _tm.tm_min, _tm.tm_sec,              \
            (long long)_ms, ##__VA_ARGS__);                   \
    fflush(stderr);                                           \
  } while (0)

// ---- Shared state ----

enum RequestType : int {
  kCheckAppUpdate = 0,
  kShowUpdateDialog = 1,
  kLoadProduct = 2,
};

// TSFN registered by the NAPI bridge at ArkTS init time.
static napi_threadsafe_function g_tsfn = nullptr;
static std::mutex g_tsfn_mutex;

static void DispatchTSFN(RequestType type) {
  std::lock_guard<std::mutex> lock(g_tsfn_mutex);
  if (!g_tsfn) {
    ZYBAPI_LOG("DispatchTSFN: TSFN not registered yet, dropping request type=%d", (int)type);
    return;
  }
  auto* data = new int(static_cast<int>(type));
  napi_status status = napi_call_threadsafe_function(
      g_tsfn, data, napi_tsfn_nonblocking);
  ZYBAPI_LOG("DispatchTSFN type=%d status=%d", (int)type, (int)status);
  if (status != napi_ok) {
    delete data;
  }
}

// Per-operation Promise resolver storage (used by resolve callbacks).
// checkAppUpdate
static std::mutex g_check_mutex;
static v8::Isolate* g_check_isolate = nullptr;
static v8::Persistent<v8::Promise::Resolver>* g_check_resolver = nullptr;
// showUpdateDialog
static std::mutex g_dialog_mutex;
static v8::Isolate* g_dialog_isolate = nullptr;
static v8::Persistent<v8::Promise::Resolver>* g_dialog_resolver = nullptr;
// loadProduct
static std::mutex g_product_mutex;
static v8::Isolate* g_product_isolate = nullptr;
static v8::Persistent<v8::Promise::Resolver>* g_product_resolver = nullptr;

// ---- Helpers ----

static void ClearResolver(v8::Persistent<v8::Promise::Resolver>** r,
                          v8::Isolate** iso) {
  if (*r) {
    (*r)->Reset();
    delete *r;
    *r = nullptr;
  }
  *iso = nullptr;
}

// ---- extern "C" exports (dlsym'd by NAPI bridge) ----

extern "C" {

__attribute__((visibility("default"))) void
LynxtronRegisterUpdateTSFN(void* env, void* tsfn) {
  std::lock_guard<std::mutex> lock(g_tsfn_mutex);
  // Release old TSFN if re-registering.
  if (g_tsfn) {
    napi_release_threadsafe_function(g_tsfn, napi_tsfn_release);
  }
  g_tsfn = static_cast<napi_threadsafe_function>(tsfn);
  ZYBAPI_LOG("LynxtronRegisterUpdateTSFN env=%p tsfn=%p", env, tsfn);
}

__attribute__((visibility("default"))) void LynxtronResolveCheckAppUpdate(const char* json) {
  ZYBAPI_LOG("ResolveCheckAppUpdateApi json=%s", json ? json : "null");
  if (!json) return;
  std::string json_copy(json);
  // Post V8 work to the Node.js main thread so we don't block ArkTS.
  auto runner = lynxtron::GetUIThreadTaskRunner();
  if (!runner) {
    ZYBAPI_LOG("ResolveCheckAppUpdate: no UI runner, dropping");
    return;
  }
  runner->PostTask(FROM_HERE, base::BindOnce([](std::string result_json) {
    std::lock_guard<std::mutex> lock(g_check_mutex);
    if (!g_check_isolate || !g_check_resolver) {
      ZYBAPI_LOG("ResolveCheckAppUpdate skipped: resolver gone");
      return;
    }
    v8::Isolate* isolate = g_check_isolate;
    v8::HandleScope hs(isolate);
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
    v8::Context::Scope cs(ctx);
    v8::Local<v8::Promise::Resolver> r =
        v8::Local<v8::Promise::Resolver>::New(isolate, *g_check_resolver);
    v8::Local<v8::String> v =
        v8::String::NewFromUtf8(isolate, result_json.c_str()).ToLocalChecked();
    v8::Local<v8::Value> parsed;
    if (v8::JSON::Parse(ctx, v).ToLocal(&parsed)) {
      r->Resolve(ctx, parsed).Check();
    } else {
      r->Resolve(ctx, v).Check();
    }
    ClearResolver(&g_check_resolver, &g_check_isolate);
    ZYBAPI_LOG("ResolveCheckAppUpdate done");
  }, std::move(json_copy)));
}


__attribute__((visibility("default"))) void LynxtronResolveShowUpdateDialog(int result_code) {
  ZYBAPI_LOG("ResolveShowUpdateDialog result_code=%d", result_code);
  auto runner = lynxtron::GetUIThreadTaskRunner();
  if (!runner) return;
  runner->PostTask(FROM_HERE, base::BindOnce([](int code) {
    std::lock_guard<std::mutex> lock(g_dialog_mutex);
    if (!g_dialog_isolate || !g_dialog_resolver) {
      return;
    }
    v8::Isolate* isolate = g_dialog_isolate;
    v8::HandleScope hs(isolate);
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
    v8::Context::Scope cs(ctx);
    v8::Local<v8::Promise::Resolver> r =
        v8::Local<v8::Promise::Resolver>::New(isolate, *g_dialog_resolver);
    r->Resolve(ctx, v8::Integer::New(isolate, code)).Check();
    ClearResolver(&g_dialog_resolver, &g_dialog_isolate);
  }, result_code));
}


__attribute__((visibility("default"))) void LynxtronResolveLoadProduct(const char* json) {
  if (!json) return;
  std::string json_copy(json);
  auto runner = lynxtron::GetUIThreadTaskRunner();
  if (!runner) return;
  runner->PostTask(FROM_HERE, base::BindOnce([](std::string result_json) {
    std::lock_guard<std::mutex> lock(g_product_mutex);
    if (!g_product_isolate || !g_product_resolver) return;
    v8::Isolate* isolate = g_product_isolate;
    v8::HandleScope hs(isolate);
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
    v8::Context::Scope cs(ctx);
    v8::Local<v8::Promise::Resolver> r =
        v8::Local<v8::Promise::Resolver>::New(isolate, *g_product_resolver);
    v8::Local<v8::String> v =
        v8::String::NewFromUtf8(isolate, result_json.c_str()).ToLocalChecked();
    v8::Local<v8::Value> parsed;
    if (v8::JSON::Parse(ctx, v).ToLocal(&parsed)) {
      r->Resolve(ctx, parsed).Check();
    } else {
      r->Resolve(ctx, v).Check();
    }
    ClearResolver(&g_product_resolver, &g_product_isolate);
  }, std::move(json_copy)));
}

}  // extern "C"

// ---- Node.js linked binding ----

namespace {

v8::Local<v8::Value> CheckAppUpdate(v8::Isolate* isolate) {
  ZYBAPI_LOG("CheckAppUpdate called");
#if !BUILDFLAG(IS_HARMONY)
  auto resolver = v8::Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();
  resolver->Reject(isolate->GetCurrentContext(),
                   v8::Exception::Error(v8::String::NewFromUtf8(isolate,
                       "checkAppUpdate is only supported on HarmonyOS").ToLocalChecked())).Check();
  return resolver->GetPromise();
#else
  v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
  auto resolver = v8::Promise::Resolver::New(ctx).ToLocalChecked();
  {
    std::lock_guard<std::mutex> lock(g_check_mutex);
    ClearResolver(&g_check_resolver, &g_check_isolate);
    g_check_isolate = isolate;
    g_check_resolver = new v8::Persistent<v8::Promise::Resolver>();
    g_check_resolver->Reset(isolate, resolver);
  }
  DispatchTSFN(kCheckAppUpdate);
  return resolver->GetPromise();
#endif
}

v8::Local<v8::Value> ShowUpdateDialog(v8::Isolate* isolate) {
#if !BUILDFLAG(IS_HARMONY)
  auto resolver = v8::Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();
  resolver->Reject(isolate->GetCurrentContext(),
                   v8::Exception::Error(v8::String::NewFromUtf8(isolate,
                       "showUpdateDialog is only supported on HarmonyOS").ToLocalChecked())).Check();
  return resolver->GetPromise();
#else
  v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
  auto resolver = v8::Promise::Resolver::New(ctx).ToLocalChecked();
  {
    std::lock_guard<std::mutex> lock(g_dialog_mutex);
    ClearResolver(&g_dialog_resolver, &g_dialog_isolate);
    g_dialog_isolate = isolate;
    g_dialog_resolver = new v8::Persistent<v8::Promise::Resolver>();
    g_dialog_resolver->Reset(isolate, resolver);
  }
  DispatchTSFN(kShowUpdateDialog);
  return resolver->GetPromise();
#endif
}

v8::Local<v8::Value> LoadProduct(v8::Isolate* isolate) {
#if !BUILDFLAG(IS_HARMONY)
  auto resolver = v8::Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();
  resolver->Reject(isolate->GetCurrentContext(),
                   v8::Exception::Error(v8::String::NewFromUtf8(isolate,
                       "loadProduct is only supported on HarmonyOS").ToLocalChecked())).Check();
  return resolver->GetPromise();
#else
  v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
  auto resolver = v8::Promise::Resolver::New(ctx).ToLocalChecked();
  {
    std::lock_guard<std::mutex> lock(g_product_mutex);
    ClearResolver(&g_product_resolver, &g_product_isolate);
    g_product_isolate = isolate;
    g_product_resolver = new v8::Persistent<v8::Promise::Resolver>();
    g_product_resolver->Reset(isolate, resolver);
  }
  DispatchTSFN(kLoadProduct);
  return resolver->GetPromise();
#endif
}

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> /*unused*/,
                v8::Local<v8::Context> context,
                void* /*priv*/) {
  v8::Isolate* isolate = context->GetIsolate();
  gin_helper::Dictionary dict(isolate, exports);
  dict.SetMethod("checkAppUpdate", &CheckAppUpdate);
  dict.SetMethod("showUpdateDialog", &ShowUpdateDialog);
  dict.SetMethod("loadProduct", &LoadProduct);
  ZYBAPI_LOG("Initialize: checkAppUpdate, showUpdateDialog, loadProduct registered");
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_binding_update_check, Initialize)
