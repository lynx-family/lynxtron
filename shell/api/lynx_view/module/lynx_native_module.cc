// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/module/lynx_native_module.h"

#include "platform/embedder/public/capi/lynx_extension_module_capi.h"
#include "shell/api/lynx_view/module/lynx_emit_event.h"
#include "shell/app/javascript_environment.h"
#include "shell/common/gin_helper/handle.h"
#include "shell/common/global_thread.h"
#include "third_party/napi/include/napi_env_v8.h"
#include "v8.h"

#ifdef USE_PRIMJS_NAPI
#include "third_party/napi/include/primjs_napi_defines.h"
#endif

namespace lynxtron {
namespace {

struct CallbackContext {
  napi_ref func_ref;
  napi_ref this_ref;
};

struct JSBCallbackData {
  std::vector<uint8_t> data;
  napi_threadsafe_function tsfn;
};

napi_threadsafe_function EncodeNapiCallBack(napi_env env,
                                            napi_value js_this,
                                            napi_value napi_callback) {
  auto* ctx = new CallbackContext();
  env->napi_create_reference(env, napi_callback, 1, &ctx->func_ref);
  env->napi_create_reference(env, js_this, 1, &ctx->this_ref);

  napi_threadsafe_function tsfn;
  env->napi_create_threadsafe_function(
      env, ctx,
      [](napi_env env, void* data, void* hint) {
        delete static_cast<CallbackContext*>(data);
      },
      ctx,
      [](napi_env env, void* ctx_ptr, void* data_ptr) {
        auto* ctx = static_cast<CallbackContext*>(ctx_ptr);
        auto* value = static_cast<JSBCallbackData*>(data_ptr);
        v8::Local<v8::Context> context = napi_get_env_context_v8(env);
        v8::Isolate* isolate = context->GetIsolate();
        v8::Isolate::Scope isolate_scope(isolate);
        v8::Context::Scope context_scope(context);

        napi_value func;
        env->napi_get_reference_value(env, ctx->func_ref, &func);
        napi_value js_this;
        env->napi_get_reference_value(env, ctx->this_ref, &js_this);

        v8::Local<v8::Value> js_value =
            DeserializeValue(isolate, context, value->data);
        napi_value argv = napi_v8_value_to_js_value(env, js_value);

        env->napi_call_function(env, js_this, func, 1, &argv, nullptr);
        env->napi_delete_reference(env, ctx->func_ref);
        env->napi_delete_reference(env, ctx->this_ref);
        delete value;
        env->napi_delete_threadsafe_function(value->tsfn);
      },
      &tsfn);

  return tsfn;
}

}  // namespace

// static
lynx_extension_module_t* LynxNativeModule::OnCreateCModule(void* opaque) {
  if (!opaque) {
    return nullptr;
  }

  auto* module_ptr = static_cast<ModulePtr*>(opaque);
  auto* module = module_ptr->get();

  module->c_module_ = lynx_extension_module_create_with_finalizer(
      module_ptr, [](lynx_extension_module_t* m, void* user_data) {
        if (user_data) {
          delete static_cast<ModulePtr*>(user_data);
        }
      });

  // Useless code, but without this, JS taskrunner cannot bind.
  lynx_extension_module_bind_runtime_init(module->c_module_,
                                          [](lynx_extension_module_t*) {});
  // Useless code, but without this, napi env not bind.
  lynx_extension_module_bind_runtime_attach(
      module->c_module_,
      [](lynx_extension_module_t*, napi_env, lynx_vsync_observer_t*) {});
  lynx_extension_module_set_napi_module_creator(module->c_module_,
                                                BindFunctionsToNapiModule);
  return module->c_module_;
}

// static
void LynxNativeModule::RegisterLynxNodeModule(lynx_view_builder_t* builder,
                                              const char* module_name,
                                              LynxNativeModule* module) {
  auto* module_ptr = new ModulePtr(module);
  lynx_view_builder_register_extension_module(builder, module_name,
                                              LynxNativeModule::OnCreateCModule,
                                              false, module_ptr);
}

// static
napi_value LynxNativeModule::BindFunctionsToNapiModule(napi_env env,
                                                       napi_value exports,
                                                       const char* module_name,
                                                       void* opaque) {
  auto* module_ptr = static_cast<ModulePtr*>(opaque);
  auto* module = module_ptr->get();

  for (const auto& [method_name, method_data] : module->methods_) {
    napi_value native_result;

    auto* method_invoker_data = new MethodInvokerData(*module_ptr, method_data);
    env->napi_create_function(
        env, method_name.c_str(), method_name.length(),
        [](napi_env env, napi_callback_info info) -> napi_value {
          size_t napi_argc = 0;
          void* data = nullptr;
          if (env->napi_get_cb_info(env, info, &napi_argc, nullptr, nullptr,
                                    &data) != napi_ok) {
            return nullptr;
          }
          const auto* method_data = static_cast<MethodInvokerData*>(data);

          napi_value js_this;
          napi_value napi_argv[napi_argc];
          if (env->napi_get_cb_info(env, info, &napi_argc, napi_argv, &js_this,
                                    nullptr) != napi_ok) {
            return nullptr;
          }

          v8::Local<v8::Context> ctx = napi_get_env_context_v8(env);
          v8::Isolate* isolate = ctx->GetIsolate();

          // Deserialize args.
          std::vector<std::vector<uint8_t>> v8_argvs(napi_argc);
          std::unordered_map<size_t, JSBCallBack> callbacks;
          // Deserialize callback. index and callback functron.
          for (size_t index = 0; index < napi_argc; index++) {
            napi_valuetype type;
            if (env->napi_typeof(env, napi_argv[index], &type) != napi_ok) {
              return nullptr;
            }

            if (type == napi_function) {
              auto tsfn = EncodeNapiCallBack(env, js_this, napi_argv[index]);
              callbacks.insert({index, {env, tsfn}});
            } else {
              v8_argvs[index] = SerializeValue(
                  isolate, ctx,
                  napi_js_value_to_v8_value(env, napi_argv[index]));
            }
          }

          GlobalThread::GetUIThreadTaskRunner()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  [](ModulePtr module,
                     std::vector<std::vector<uint8_t>> v8_argvs,
                     std::unordered_map<size_t, JSBCallBack> callbacks,
                     MethodInvoker invoker) {
                    module->RunMethodInMainThread(std::move(invoker),
                                                  std::move(v8_argvs),
                                                  std::move(callbacks));
                  },
                  method_data->first, std::move(v8_argvs), std::move(callbacks),
                  method_data->second));

          return nullptr;
        },
        method_invoker_data, &native_result);
    env->napi_add_finalizer(
        env, native_result, method_invoker_data,
        [](napi_env env, void* data, void* hint) {
          delete static_cast<MethodInvokerData*>(data);
        },
        nullptr, nullptr);
    env->napi_set_named_property(env, exports, method_name.c_str(),
                                 native_result);
  }

  return exports;
}

void LynxNativeModule::RunMethodInMainThread(
    MethodInvoker invoker,
    std::vector<std::vector<uint8_t>> v8_argvs,
    std::unordered_map<size_t, JSBCallBack> callbacks) {
  // Deserialize args.
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::Locker locker(isolate);
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Value> params[v8_argvs.size()];
  for (size_t index = 0; index < v8_argvs.size(); index++) {
    if (callbacks.find(index) != callbacks.end()) {
      params[index] =
          LynxEmitEvent::Create(isolate, [callback = callbacks[index]](
                                             v8::Isolate* isolate,
                                             v8::Local<v8::Value> arg) {
            v8::Local<v8::Context> context = isolate->GetCurrentContext();
            v8::Context::Scope context_scope(context);

            auto data = SerializeValue(isolate, context, arg);
            auto value_ptr =
                new JSBCallbackData{std::move(data), callback.tsfn};

            auto status = callback.env->napi_call_threadsafe_function(
                callback.tsfn, value_ptr, napi_tsfn_nonblocking);
            if (status != napi_ok) {
              delete value_ptr;
              callback.env->napi_delete_threadsafe_function(callback.tsfn);
            }
          }).ToV8();
      continue;
    }
    params[index] = DeserializeValue(isolate, context, v8_argvs[index]);
  }

  invoker(this, isolate, context, params, v8_argvs.size());
}

void LynxNativeModule::RegisterMethod(std::string method_name,
                                      MethodInvoker invoker) {
  methods_[std::move(method_name)] = invoker;
}

}  // namespace lynxtron
