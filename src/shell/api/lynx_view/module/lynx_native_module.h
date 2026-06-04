// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_MODULE_LYNX_NATIVE_MODULE_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_MODULE_LYNX_NATIVE_MODULE_H_

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "lynx/platform/embedder/public/capi/lynx_view_builder_capi.h"
#include "shell/api/lynx_view/module/lynx_module_utils.h"
#include "third_party/weak-node-api/headers/node_api.h"

#ifdef USE_WEAK_SUFFIX_NAPI
#include "third_party/weak-node-api/headers/weak_napi_defines.h"
#endif

namespace v8 {
class Isolate;
class Context;
class Value;
template <typename T>
class Local;
}  // namespace v8

namespace lynxtron {

class LynxNativeModule {
 public:
  // isolate, context, args, argc
  using ModulePtr = std::shared_ptr<LynxNativeModule>;
  using MethodInvoker = void (*)(LynxNativeModule*,
                                 v8::Isolate*,
                                 v8::Local<v8::Context>,
                                 v8::Local<v8::Value> const*,
                                 int);
  using MethodInvokerData = std::pair<ModulePtr, MethodInvoker>;
  struct JSBCallBack {
    napi_env env;
    napi_threadsafe_function tsfn;
    std::shared_ptr<std::atomic_bool> released;
  };

  // bind the module to napi and return cmodule pointer.
  static lynx_extension_module_t* OnCreateCModule(void* opaque);

  static void RegisterLynxNodeModule(lynx_view_builder_t* builder,
                                     const char* module_name,
                                     LynxNativeModule* module);

  LynxNativeModule() = default;
  virtual ~LynxNativeModule() = default;
  void RegisterMethod(std::string method_name, MethodInvoker invoker);

 protected:
  // static method to create napi module.
  static napi_value BindFunctionsToNapiModule(napi_env env,
                                              napi_value exports,
                                              const char* module_name,
                                              void* opaque);

  void RunMethodInMainThread(MethodInvoker invoker,
                             std::vector<std::vector<uint8_t>> v8_argvs,
                             std::unordered_map<size_t, JSBCallBack> callbacks);

  // method invoker must run on main thread.
  std::unordered_map<std::string, MethodInvoker> methods_;
  lynx_extension_module_t* c_module_ = nullptr;
};

#ifdef USE_WEAK_SUFFIX_NAPI
#include "third_party/weak-node-api/headers/weak_napi_undefs.h"
#endif

}  // namespace lynxtron
#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_MODULE_LYNX_NATIVE_MODULE_H_
