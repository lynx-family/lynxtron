// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/module/lynx_node_module.h"

#include <memory>
#include <utility>

#include "build/build_config.h"

#include "base/include/fml/message_loop.h"
#if BUILDFLAG(IS_HARMONY)
#include "base/include/fml/platform/harmony/message_loop_harmony.h"
#else
#include "base/include/fml/platform/node/message_loop_node.h"
#endif
#include "node.h"

// base_export.h might be included by lynx/lynx/base headers and redefine
// BASE_EXPORT. We undefine it here to avoid a redefinition warning from
// chromium base/logging.h.
#undef BASE_EXPORT

#include "base/logging.h"
#include "lynx/platform/embedder/public/capi/lynx_env_capi.h"
#include "platform/embedder/public/lynx_extension_module.h"
#include "shell/common/node_includes.h"
#include "shell/common/node_util.h"
#include "third_party/napi/include/napi_env_v8.h"

#ifdef USE_WEAK_SUFFIX_NAPI
#include "third_party/weak-node-api/headers/weak_napi_defines.h"
#endif

namespace lynxtron {
namespace {
static void* v8_platform_ = nullptr;

const uint64_t kInitNodeEnvflags =
    node::EnvironmentFlags::kHideConsoleWindows |
    node::EnvironmentFlags::kNoStartDebugSignalHandler |
    node::EnvironmentFlags::kNoCreateInspector |
    node::EnvironmentFlags::kNoWaitForInspectorFrontend |
    node::EnvironmentFlags::kNoGlobalSearchPaths;

uv_loop_t* GetUVLoopFromCurrent() {
#if BUILDFLAG(IS_HARMONY)
  // HarmonyOS keeps lynx's own MessageLoopHarmony as the loop implementation
  // for this thread -- message_loop_node.cc is only built for the desktop
  // platforms -- so the uv loop has to be read back through it. It is a real
  // uv_loop_t from Node's libuv, created by MessageLoopHarmony itself.
  auto* message_loop = reinterpret_cast<lynx::fml::MessageLoopHarmony*>(
      lynx::fml::MessageLoop::GetCurrent().GetLoopImpl().get());
  return reinterpret_cast<uv_loop_t*>(message_loop->GetPlatformLoop());
#else
  auto* message_loop = reinterpret_cast<lynx::fml::MessageLoopNode*>(
      lynx::fml::MessageLoop::GetCurrent().GetLoopImpl().get());
  return message_loop->GetUVLoop();
#endif
}

v8::Local<v8::Value> RunFunctionInNodeContext(v8::Isolate* v8_isolate,
                                              v8::Local<v8::Context> node_ctx,
                                              const char* script,
                                              size_t args,
                                              v8::Local<v8::Value>* argv) {
  v8::TryCatch try_catch{v8_isolate};
  v8::Local<v8::Value> ret;

  v8::Local<v8::String> v8_script_str;
  if (!v8::String::NewFromUtf8(v8_isolate, script).ToLocal(&v8_script_str)) {
    return v8::Undefined(v8_isolate);
  }

  v8::Local<v8::Script> v8_script;
  if (!v8::Script::Compile(node_ctx, v8_script_str).ToLocal(&v8_script)) {
    return v8::Undefined(v8_isolate);
  }

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
    v8::Local<v8::String> msg_str;
    if (!try_catch.Message().IsEmpty() &&
        try_catch.Message()->Get()->ToString(node_ctx).ToLocal(&msg_str)) {
      msg = *v8::String::Utf8Value(v8_isolate, msg_str);
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
      Napi::Env env,
      std::unique_ptr<lynx::pub::VSyncObserver> vsync_observer) override;
  void OnRuntimeReady(Napi::Env env,
                      Napi::Value lynx,
                      const char* url) override;

  void OnRuntimeDetach() override;

 private:
  v8::Local<v8::Context> CreateNewNodeContext(v8::Isolate* v8_isolate);
  bool CheckModuleData();

  static void FlushMicrotasks(const v8::FunctionCallbackInfo<v8::Value>& args);

  // module data
  NodeModuleData* module_data_ = nullptr;

  // node environment data
  node::Environment* env_ = nullptr;
  node::IsolateData* isolate_data_ = nullptr;

  // node exports and context
  v8::Global<v8::Value> node_exports_;
  v8::Global<v8::Context> node_context_;
  // Node preload Promises settle from the Node context, but callers observe
  // them from Lynx. Keep the Lynx context so the Node-side callback can drain
  // the right microtask queue after forwarding resolution.
  v8::Global<v8::Context> lynx_context_;
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

  if (env_) {
    node::FreeEnvironment(env_);
  }
  if (isolate_data_) {
    node::FreeIsolateData(isolate_data_);
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
  module->SetNapiModuleCreator([](Napi::Env env, Napi::Value exports,
                                  const char* module_name,
                                  LynxNodeModule& module) {
    if (!module.node_exports_.IsEmpty()) {
      napi_env c_env = static_cast<napi_env>(env);
      napi_env_primjs primjs_env = reinterpret_cast<napi_env_primjs>(c_env);
      auto v8_context = napi_get_env_context_v8(primjs_env);
      auto v8_isolate = v8_context->GetIsolate();
      module.lynx_context_.Reset(v8_isolate, v8_context);

      napi_value napi_api =
          reinterpret_cast<napi_value>(napi_v8_value_to_js_value(
              primjs_env, module.node_exports_.Get(v8_isolate)));

      exports.As<Napi::Object>().Set("exposed", Napi::Value(env, napi_api));
    } else {
      LOG(ERROR) << "NapiModuleCreator: node_exports_ is empty";
    }
    return exports;
  });
  return c_module;
}

v8::Local<v8::Context> LynxNodeModule::CreateNewNodeContext(
    v8::Isolate* v8_isolate) {
  auto new_context = node::NewContext(v8_isolate);
  v8::Context::Scope context_scope(new_context);

  isolate_data_ = node::CreateIsolateData(
      v8_isolate, GetUVLoopFromCurrent(),
      reinterpret_cast<node::MultiIsolatePlatform*>(v8_platform_));

  // IMPORTANT: Always provide a "main script" as argv[1]. If we don't,
  // upstream Node.js may fall back to interactive REPL mode when stdin is a
  // TTY. The embedded Node environment used for BTS preloads should never
  // start a REPL.
  std::vector<std::string> args = {"lynxbts_node",
                                   "lynxtron/js2c/lynxbts_init"};
  env_ = node::CreateEnvironment(
      isolate_data_, new_context, std::move(args), {},
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

// static
void LynxNodeModule::FlushMicrotasks(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Data().IsEmpty() || !args.Data()->IsExternal()) {
    return;
  }

  auto* module =
      static_cast<LynxNodeModule*>(args.Data().As<v8::External>()->Value());
  if (!module || module->lynx_context_.IsEmpty()) {
    return;
  }

  v8::Isolate* v8_isolate = args.GetIsolate();
  // uv_run only drives Node's work queue. The wrapper Promise returned to Lynx
  // uses Lynx's microtask queue, so drain that queue explicitly after the Node
  // thenable settles.
  module->lynx_context_.Get(v8_isolate)
      ->GetMicrotaskQueue()
      ->PerformCheckpoint(v8_isolate);
}

bool LynxNodeModule::CheckModuleData() {
  if (!module_data_) {
    return false;
  }
  return !module_data_->preload_paths.empty();
}

void LynxNodeModule::OnRuntimeAttach(
    Napi::Env env,
    std::unique_ptr<lynx::pub::VSyncObserver> vsync_observer) {
  if (!CheckModuleData()) {
    return;
  }
  napi_env c_env = static_cast<napi_env>(env);
  auto v8_context =
      napi_get_env_context_v8(reinterpret_cast<napi_env_primjs>(c_env));
  auto v8_isolate = v8_context->GetIsolate();
  lynx_context_.Reset(v8_isolate, v8_context);

  v8::Isolate::Scope isolate_scope(v8_isolate);
  v8::HandleScope handle_scope(v8_isolate);

  auto node_ctx = CreateNewNodeContext(v8_isolate);
  v8::Context::Scope context_scope(node_ctx);
  node_context_ = v8::Global<v8::Context>(v8_isolate, node_ctx);

  v8::Local<v8::Value> console;
  if (!v8_context->Global()
           ->Get(v8_context,
                 v8::String::NewFromUtf8Literal(v8_isolate, "console"))
           .ToLocal(&console)) {
    console = v8::Undefined(v8_isolate);
  }

  v8::Local<v8::Array> preload_paths = v8::Array::New(v8_isolate);
  for (size_t i = 0; i < module_data_->preload_paths.size(); ++i) {
    v8::Local<v8::String> path_str;
    if (v8::String::NewFromUtf8(v8_isolate,
                                module_data_->preload_paths[i].c_str())
            .ToLocal(&path_str)) {
      [[maybe_unused]] auto _ = preload_paths->Set(node_ctx, i, path_str);
    }
  }

  // This callback is created in the Lynx context so Node can trigger a Lynx
  // microtask checkpoint without knowing about the target context.
  v8::Local<v8::Function> flush_microtasks;
  if (!v8::Function::New(v8_context, &LynxNodeModule::FlushMicrotasks,
                         v8::External::New(v8_isolate, this), 1,
                         v8::ConstructorBehavior::kThrow)
           .ToLocal(&flush_microtasks)) {
    flush_microtasks =
        v8::Function::New(v8_context,
                          [](const v8::FunctionCallbackInfo<v8::Value>&) {})
            .ToLocalChecked();
  }

  v8::Local<v8::Value> bridge_runtime_argv[1] = {flush_microtasks};
  // Return a Promise allocated in the Lynx context so caller-side await/.then
  // uses the same realm as the consuming Lynx code.
  v8::Local<v8::Value> bridge_runtime =
      RunFunctionInNodeContext(v8_isolate, v8_context, R"((flushMicrotasks)=>{
    return Object.freeze({
      createPromise(executor) {
        return new Promise(executor);
      },
      flushMicrotasks,
    });
  })",
                               1, bridge_runtime_argv);

  // init context bridge
  const char* const kContextBridgeInitScriptSource = R"(
    (console, preload_paths, bridge_runtime)=>{
      const bts = globalThis.__lynxtronBTS;
      if (!bts || typeof bts !== 'object') {
        throw new Error('__lynxtronBTS is not found');
      }
      bts.setupLynxtronBTS(console, preload_paths, bridge_runtime);
      return bts.getLynxtronBTSBridgeData();
    }
  )";
  v8::Local<v8::Value> v8_argv[3] = {console, preload_paths, bridge_runtime};

  v8::Local<v8::Value> exportsData = RunFunctionInNodeContext(
      v8_isolate, node_ctx, kContextBridgeInitScriptSource, 3, v8_argv);
  node_exports_.Reset(v8_isolate, exportsData);
}

void LynxNodeModule::OnRuntimeReady(Napi::Env env,
                                    Napi::Value lynx,
                                    const char* url) {}

void LynxNodeModule::OnRuntimeDetach() {
  lynx_context_.Reset();
  if (env_) {
    node::FreeEnvironment(env_);
    env_ = nullptr;
  }
  if (isolate_data_) {
    node::FreeIsolateData(isolate_data_);
    isolate_data_ = nullptr;
  }
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
