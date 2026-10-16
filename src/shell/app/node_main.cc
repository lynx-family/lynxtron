// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/node_main.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/buildflag.h"
#include "shell/app/javascript_environment.h"
#include "shell/app/uv_stdio_fix.h"
#include "shell/common/lynxtron_command_line.h"
#include "shell/common/lynxtron_paths.h"
#include "shell/common/node_bindings.h"
#include "shell/common/node_includes.h"
#include "shell/common/node_util.h"
#include "shell/common/path_provider.h"
#include "third_party/node/src/node_snapshot_builder.h"
#include "uv.h"
#include "v8-isolate.h"
#include "v8-local-handle.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "shell/common/mac/main_application_bundle.h"
#endif

namespace lynxtron {

int RunNodeMain() {
  std::vector<std::string> args = lynxtron::LynxtronCommandLine::AsUtf8();
  uint64_t process_flags =
      node::ProcessInitializationFlags::kNoInitializeV8 |
      node::ProcessInitializationFlags::kNoInitializeNodeV8Platform |
      node::ProcessInitializationFlags::kEnableStdioInheritance;

  lynxtron::NodeBindings::RegisterBuiltinBindings();

  std::shared_ptr<node::InitializationResult> result =
      node::InitializeOncePerProcess(
          args,
          static_cast<node::ProcessInitializationFlags::Flags>(process_flags));

  for (const std::string& error : result->errors()) {
    std::cerr << args[0] << ": " << error << '\n';
  }

  if (result->early_return()) {
    return result->exit_code();
  }

#if BUILDFLAG(IS_MAC)
  FixStdioStreams();
  base::apple::SetOverrideFrameworkBundlePath(
      lynxtron::MainApplicationBundlePath()
          .Append("Contents")
          .Append("Frameworks")
          .Append(LYNXTRON_PRODUCT_NAME " Framework.framework"));
  base::apple::SetOverrideOuterBundlePath(
      lynxtron::MainApplicationBundlePath());
#endif

  base::PathService::RegisterProvider(lynxtron::PathProvider,
                                      lynxtron::PATH_START, lynxtron::PATH_END);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("lynxtron");
  base::SingleThreadTaskExecutor task_executor(base::MessagePumpType::DEFAULT,
                                               true);
  const node::SnapshotData* snapshot =
      node::SnapshotBuilder::GetEmbeddedSnapshotData();
  CHECK(snapshot);
  v8::V8::SetSnapshotDataBlob(
      const_cast<v8::StartupData*>(&snapshot->v8_snapshot_blob_data));

  int exit_code = 1;
  {
    uv_loop_t* loop = uv_default_loop();
    bool setup_wasm_streaming =
        node::per_process::cli_options->get_per_isolate_options()
            ->get_per_env_options()
            ->experimental_fetch;

    lynxtron::JavascriptEnvironment js_env(loop, setup_wasm_streaming);
    v8::Isolate* isolate = js_env.isolate();
    v8::HandleScope scope(isolate);

    auto snapshot_wrapper = snapshot->AsEmbedderWrapper();
    node::IsolateData* isolate_data = node::CreateIsolateData(
        isolate, loop, js_env.platform(), nullptr, snapshot_wrapper.get());

    uint64_t env_flags = node::EnvironmentFlags::kDefaultFlags |
                         node::EnvironmentFlags::kHideConsoleWindows;

    node::Environment* env = lynxtron::util::CreateEnvironment(
        isolate, isolate_data, v8::Local<v8::Context>(), result->args(),
        result->exec_args(),
        static_cast<node::EnvironmentFlags::Flags>(env_flags));

    if (env) {
      js_env.EnterContext(env->context());
      node::SetIsolateUpForNode(isolate);
      node::LoadEnvironment(env, node::StartExecutionCallback{},
                            &lynxtron::OnNodePreload);

      exit_code = node::SpinEventLoop(env).FromMaybe(1);

      node::ResetStdio();
      node::Stop(env, node::StopFlags::kDoNotTerminateIsolate);
      node::FreeEnvironment(env);
    }
    node::FreeIsolateData(isolate_data);
  }

  node::TearDownOncePerProcess();
  base::ThreadPoolInstance::Get()->Shutdown();

  return exit_code;
}

}  // namespace lynxtron
