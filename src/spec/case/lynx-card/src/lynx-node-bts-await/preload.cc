// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <node_api.h>

#include <cstdlib>
#include <thread>

namespace {

void Check(napi_status status) {
  if (status != napi_ok) {
    std::abort();
  }
}

struct Job {
  napi_deferred deferred;
  bool called = false;
};

void Complete(napi_env env, napi_value, void* context, void*) {
  if (env != nullptr) {
    static_cast<Job*>(context)->called = true;
  }
}

void Finalize(napi_env env, void* data, void*) {
  auto* job = static_cast<Job*>(data);
  if (env != nullptr) {
    napi_value result;
    Check(napi_get_boolean(env, job->called, &result));
    Check(napi_resolve_deferred(env, job->deferred, result));
  }
  delete job;
}

napi_value Run(napi_env env, napi_callback_info) {
  auto* job = new Job;
  napi_value promise;
  Check(napi_create_promise(env, &job->deferred, &promise));
  napi_value name;
  Check(napi_create_string_utf8(env, "preloadTSFN", NAPI_AUTO_LENGTH, &name));
  napi_threadsafe_function tsfn;
  Check(napi_create_threadsafe_function(env, nullptr, nullptr, name, 0, 1, job,
                                        Finalize, job, Complete, &tsfn));

  // Resolve from the finalizer so one test covers both dispatch and uv_close.
  // The worker keeps only the handle; Finalize owns Job.
  std::thread([tsfn]() {
    Check(napi_call_threadsafe_function(tsfn, nullptr, napi_tsfn_nonblocking));
    Check(napi_release_threadsafe_function(tsfn, napi_tsfn_release));
  }).detach();
  return promise;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_value run;
  Check(napi_create_function(env, "run", NAPI_AUTO_LENGTH, Run, nullptr, &run));
  Check(napi_set_named_property(env, exports, "run", run));
  return exports;
}

}  // namespace

NAPI_MODULE(preload, Init)
