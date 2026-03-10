// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/module/lynx_node_module.h"

#include <memory>

#include "base/include/fml/message_loop.h"
#include "base/include/fml/platform/node/message_loop_node.h"
#include "base/logging.h"
#include "lynx/platform/embedder/public/capi/lynx_env_capi.h"
#include "platform/embedder/public/lynx_extension_module.h"
#include "shell/common/node_includes.h"
#include "shell/common/node_util.h"
#include "third_party/napi/include/napi_env_v8.h"

#ifdef USE_PRIMJS_NAPI
#include "third_party/napi/include/primjs_napi_defines.h"
#endif

namespace lynxtron {
namespace {
static void* v8_platform_ = nullptr;

const uint64_t kInitNodeEnvflags =
    node::EnvironmentFlags::kNoBrowserGlobals |
    node::EnvironmentFlags::kHideConsoleWindows |
    node::EnvironmentFlags::kNoStartDebugSignalHandler |
    node::EnvironmentFlags::kNoCreateInspector |
    node::EnvironmentFlags::kNoWaitForInspectorFrontend |
    node::EnvironmentFlags::kNoGlobalSearchPaths;

uv_loop_t* GetUVLoopFromCurrent() {
  auto* message_loop = reinterpret_cast<lynx::fml::MessageLoopNode*>(
      lynx::fml::MessageLoop::GetCurrent().GetLoopImpl().get());
  return message_loop->GetUVLoop();
}

v8::Local<v8::Value> RunFunctionInNodeContext(v8::Isolate* v8_isolate,
                                              v8::Local<v8::Context> node_ctx,
                                              const char* script,
                                              size_t args,
                                              v8::Local<v8::Value>* argv) {
  v8::TryCatch try_catch{v8_isolate};
  v8::Local<v8::Value> ret;

  auto v8_script_str =
      v8::String::NewFromUtf8(v8_isolate, script).ToLocalChecked();

  auto v8_script =
      v8::Script::Compile(node_ctx, v8_script_str).ToLocalChecked();

  auto function_maybe = v8_script->Run(node_ctx);
  v8::Local<v8::Value> function_val;
  if (!function_maybe.ToLocal(&function_val)) {
    // Run failed
    return v8::Undefined(v8_isolate);
  }

  v8::Local<v8::Function> function =
      v8::Local<v8::Function>::Cast(function_val);

  auto ret_maybe = function->Call(node_ctx, node_ctx->Global(), args, argv);

  if (try_catch.HasCaught()) {
    std::string msg = "no error message";
    if (!try_catch.Message().IsEmpty()) {
      msg = *v8::String::Utf8Value(
          v8_isolate,
          try_catch.Message()->Get()->ToString(node_ctx).ToLocalChecked());
    } else if (try_catch.HasTerminated()) {
      msg = "script execution has been terminated";
    }
    LOG(ERROR) << msg;
  }
  if (ret_maybe.ToLocal(&ret)) {
    return ret;
  }

  return v8::Undefined(v8_isolate);
}

}  // namespace

void SetNodePlatformEnvToLynxNodeModule(void* v8_platform) {
  v8_platform_ = v8_platform;
}

class LynxNodeModule : public lynx::pub::LynxExtensionModule {
 public:
  LynxNodeModule(void* data);
  ~LynxNodeModule() override;

  static lynx_extension_module_t* CreateLynxNodeModule(void* opaque);

  struct NodeModuleData {
    std::vector<std::string> preload_paths;
  };

  void OnRuntimeAttach(
      napi_env env,
      std::unique_ptr<lynx::pub::VSyncObserver> vsync_observer) override;
  void OnRuntimeReady(napi_env env, napi_value lynx, const char* url) override;

 private:
  v8::Local<v8::Context> CreateNewNodeContext(v8::Isolate* v8_isolate);
  bool CheckModuleData();

  // module data
  NodeModuleData* module_data_ = nullptr;

  // node environment data
  node::Environment* env_ = nullptr;
  node::IsolateData* isolate_data_ = nullptr;

  // node exports and context
  v8::Global<v8::Value> node_exports_;
  v8::Global<v8::Context> node_context_;
};

LynxNodeModule::LynxNodeModule(void* data) {
  if (data) {
    module_data_ = reinterpret_cast<NodeModuleData*>(data);
  }
}

LynxNodeModule::~LynxNodeModule() {
  if (module_data_) {
    delete module_data_;
  }

  if (isolate_data_) {
    node::FreeIsolateData(isolate_data_);
  }
  if (env_) {
    node::FreeEnvironment(env_);
  }
}

// static
lynx_extension_module_t* LynxNodeModule::CreateLynxNodeModule(void* opaque) {
  auto* module = new LynxNodeModule(opaque);
  lynx_extension_module_t* c_module =
      lynx_extension_module_create_with_finalizer(
          module, [](lynx_extension_module_t* m, void* user_data) {
            if (user_data) {
              delete reinterpret_cast<LynxNodeModule*>(user_data);
            }
          });

  module->SetCModule(c_module);
  module->SetNapiModuleCreator([](napi_env env, napi_value exports,
                                  const char* module_name,
                                  void* opaque) { return exports; });
  return c_module;
}

v8::Local<v8::Context> LynxNodeModule::CreateNewNodeContext(
    v8::Isolate* v8_isolate) {
  auto new_context = node::NewContext(v8_isolate);
  v8::Context::Scope context_scope(new_context);

  isolate_data_ = node::CreateIsolateData(
      v8_isolate, GetUVLoopFromCurrent(),
      reinterpret_cast<node::MultiIsolatePlatform*>(v8_platform_));

  env_ = node::CreateEnvironment(
      isolate_data_, new_context, {}, {},
      static_cast<node::EnvironmentFlags::Flags>(kInitNodeEnvflags));

  node::LoadEnvironment(env_, node::StartExecutionCallback{},
                        [](node::Environment* env, v8::Local<v8::Value> process,
                           v8::Local<v8::Value> require) {
                          RunFunctionInNodeContext(
                              env->isolate(), env->context(), R"((require)=>{
      const { setupLynxtronBTS, getLynxtronBTSBridgeData } = require('lynxtron/js2c/lynxbts_init');
      const bts = Object.freeze({ setupLynxtronBTS, getLynxtronBTSBridgeData });
      Object.defineProperty(globalThis, '__lynxtronBTS', {
        value: bts,
        enumerable: false,
        configurable: false,
        writable: false,
      });
    })",
                              1, &require);
                        });
  return new_context;
}

bool LynxNodeModule::CheckModuleData() {
  if (!module_data_) {
    return false;
  }
  return !module_data_->preload_paths.empty();
}

void LynxNodeModule::OnRuntimeAttach(
    napi_env env,
    std::unique_ptr<lynx::pub::VSyncObserver> vsync_observer) {
  if (!CheckModuleData()) {
    return;
  }

  auto v8_context = napi_get_env_context_v8(env);
  auto v8_isolate = v8_context->GetIsolate();

  v8::Locker locker(v8_isolate);
  v8::Isolate::Scope isolate_scope(v8_isolate);
  v8::HandleScope handle_scope(v8_isolate);

  auto node_ctx = CreateNewNodeContext(v8_isolate);
  v8::Context::Scope context_scope(node_ctx);
  node_context_ = v8::Global<v8::Context>(v8_isolate, node_ctx);

  auto console = v8_context->Global()
                     ->Get(v8_context, v8::String::NewFromUtf8Literal(
                                           v8_isolate, "console"))
                     .ToLocalChecked();

  v8::Local<v8::Array> preload_paths = v8::Array::New(v8_isolate);
  for (size_t i = 0; i < module_data_->preload_paths.size(); ++i) {
    auto path_str = v8::String::NewFromUtf8(
                        v8_isolate, module_data_->preload_paths[i].c_str())
                        .ToLocalChecked();
    preload_paths->Set(node_ctx, i, path_str).Check();
  }

  // init context bridge
  const char* const kContextBridgeInitScriptSource = R"(
    (console, preload_paths)=>{
      const bts = globalThis.__lynxtronBTS;
      if (!bts || typeof bts !== 'object') {
        throw new Error('__lynxtronBTS is not found');
      }
      bts.setupLynxtronBTS(console, preload_paths);
      return bts.getLynxtronBTSBridgeData();
    }
  )";
  v8::Local<v8::Value> v8_argv[2] = {console, preload_paths};

  v8::Local<v8::Value> exportsData = RunFunctionInNodeContext(
      v8_isolate, node_ctx, kContextBridgeInitScriptSource, 2, v8_argv);
  node_exports_.Reset(v8_isolate, exportsData);
}

void LynxNodeModule::OnRuntimeReady(napi_env env,
                                    napi_value lynx,
                                    const char* url) {
  if (!CheckModuleData() || node_exports_.IsEmpty()) {
    LOG(ERROR) << "OnRuntimeReady: node_exports_ is empty, url: " << url;
    return;
  }
  auto v8_context = napi_get_env_context_v8(env);
  auto v8_isolate = v8_context->GetIsolate();
  napi_value napi_api =
      napi_v8_value_to_js_value(env, node_exports_.Get(v8_isolate));

  constexpr const char* kInitNodeNativeApi = R"(
    (lynx, api) => {
      lynx.getNativeApp().nativeModuleProxy.nodejs.getExposed = () => api;
    }
  )";
  napi_value test_func;
  env->napi_run_script(env, kInitNodeNativeApi, strlen(kInitNodeNativeApi),
                       nullptr, &test_func);

  napi_value args[2] = {lynx, napi_api};
  env->napi_call_function(env, test_func, test_func, 2, args, nullptr);
}

void RegisterLynxNodeModuleToLynxView(
    lynx_view_builder_t* builder,
    const std::vector<std::string>& node_integration_preload) {
  void* data = new LynxNodeModule::NodeModuleData{
      .preload_paths = node_integration_preload,
  };

  lynx_view_builder_register_extension_module(
      builder, "nodejs", LynxNodeModule::CreateLynxNodeModule, false, data);
}

}  // namespace lynxtron
