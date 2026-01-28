// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/lynx_view_holder/lynx_view_node_module.h"

#include <memory>

#include "base/include/fml/message_loop.h"
#include "base/include/fml/platform/node/message_loop_node.h"
#include "lynx/platform/embedder/public/capi/lynx_env_capi.h"
#include "lynx_view_extension_register.h"
#include "platform/embedder/public/lynx_extension_module.h"
#include "third_party/napi/include/napi_env_v8.h"
#include "third_party/node/src/node.h"

// clang-format off
#include "third_party/napi/include/primjs_napi_defines.h"
// clang-format on

namespace lynxtron {
namespace {
static void* v8_platform_ = nullptr;

const uint64_t kInitNodeEnvflags =
    node::EnvironmentFlags::kNoBrowserGlobals |
    node::EnvironmentFlags::kNoStartDebugSignalHandler |
    node::EnvironmentFlags::kNoCreateInspector |
    node::EnvironmentFlags::kNoWaitForInspectorFrontend;

struct NodeEnvironmentData {
  ~NodeEnvironmentData();

  v8::Isolate* isolate_ = nullptr;
  node::Environment* env_ = nullptr;
  node::IsolateData* isolate_data_ = nullptr;
  v8::Global<v8::Value> node_exports_;
  v8::Global<v8::Context> node_context_;
};

NodeEnvironmentData::~NodeEnvironmentData() {
  if (isolate_data_) {
    node::FreeIsolateData(isolate_data_);
  }
  if (env_) {
    node::FreeEnvironment(env_);
  }

  node_exports_.Reset();
  node_context_.Reset();
}

uv_loop_t* GetUVLoopFromMessageLoop() {
  auto* message_loop = reinterpret_cast<lynx::fml::MessageLoopNode*>(
      lynx::fml::MessageLoop::GetCurrent().GetLoopImpl().get());
  return message_loop->GetUVLoop();
}

NodeEnvironmentData* CreateOrGetNodeEnvData(v8::Isolate* isolate) {
  thread_local std::unique_ptr<NodeEnvironmentData> node_env_ = nullptr;

  if (node_env_ && node_env_->isolate_ == isolate) {
    return node_env_.get();
  }

  node_env_ = std::make_unique<NodeEnvironmentData>();
  node_env_->isolate_ = isolate;

  auto uv_loop = GetUVLoopFromMessageLoop();
  {
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    auto new_context = node::NewContext(isolate);
    v8::Context::Scope context_scope(new_context);

    node_env_->isolate_data_ = node::CreateIsolateData(
        isolate, uv_loop,
        reinterpret_cast<node::MultiIsolatePlatform*>(v8_platform_));

    node_env_->env_ = node::CreateEnvironment(
        node_env_->isolate_data_, new_context, {}, {},
        static_cast<node::EnvironmentFlags::Flags>(kInitNodeEnvflags));

    static constexpr const char* script =
        "return {require:require, process:process};";
    auto node_exports =
        node::LoadEnvironment(node_env_->env_, script).ToLocalChecked();
    node_env_->node_exports_ = v8::Global<v8::Value>(isolate, node_exports);
    node_env_->node_context_ = v8::Global<v8::Context>(isolate, new_context);
  }

  return node_env_.get();
}
}  // namespace

class LynxNodeModule : public lynx::pub::LynxExtensionModule {
 public:
  LynxNodeModule() {}
  ~LynxNodeModule() override {}

  static lynx_extension_module_t* CreateLynxNodeModule(void* opaque);

  //   void OnLynxViewCreate(lynx_view_t* lynx_view) override;
  void OnRuntimeAttach(
      napi_env env,
      std::unique_ptr<lynx::pub::VSyncObserver> vsync_observer) override;

 private:
  // node::Environment* env_ = nullptr;
  // node::IsolateData* isolate_data_ = nullptr;
};

napi_value LynxNodeModuleMethodsBinder(napi_env env,
                                       napi_value exports,
                                       const char* module_name,
                                       void* opaque) {
  return exports;
}

// static
lynx_extension_module_t* LynxNodeModule::CreateLynxNodeModule(void* opaque) {
  auto* module = new LynxNodeModule();
  lynx_extension_module_t* c_module =
      lynx_extension_module_create_with_finalizer(
          module, [](lynx_extension_module_t* m, void* user_data) {
            if (user_data) {
              delete reinterpret_cast<LynxNodeModule*>(user_data);
            }
          });

  module->SetCModule(c_module);
  module->SetNapiModuleCreator(&LynxNodeModuleMethodsBinder);
  return c_module;
}

void LynxNodeModule::OnRuntimeAttach(
    napi_env env,
    std::unique_ptr<lynx::pub::VSyncObserver> vsync_observer) {
  auto v8_context = napi_get_env_context_v8(env);
  auto v8_isolate = v8_context->GetIsolate();

  v8::Locker locker(v8_isolate);
  v8::Isolate::Scope isolate_scope(v8_isolate);
  v8::HandleScope handle_scope(v8_isolate);

  auto* node_env_data = CreateOrGetNodeEnvData(v8_isolate);

  v8::Context::Scope context_scope(v8_context);
  v8_context->Global()
      ->Set(v8_context,
            v8::String::NewFromUtf8Literal(v8_isolate, "__node_exports__"),
            node_env_data->node_exports_.Get(v8_isolate))
      .FromJust();
}

void SetNodePlatformEnvToLynxNodeModule(void* v8_platform) {
  v8_platform_ = v8_platform;
}

void RegisterLynxNodeModuleToLynxView(lynx_view_builder_t* builder) {
  lynx_view_builder_register_extension_module(
      builder, "NodeJS", LynxNodeModule::CreateLynxNodeModule, false, nullptr);
}

void RegisterLynxNodeModuleGlobal() {
  lynx_env_register_extension_module(
      "NodeJS", LynxNodeModule::CreateLynxNodeModule, false, nullptr);
}

}  // namespace lynxtron
