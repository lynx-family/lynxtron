// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/library_main.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "gin/v8_initializer.h"
#include "shell/app/javascript_environment.h"
#include "shell/app/main_runner.h"
#include "shell/app/relauncher.h"
#include "shell/app/uv_stdio_fix.h"
#include "shell/common/logging.h"
#include "shell/common/lynxtron_command_line.h"
#include "shell/common/lynxtron_paths.h"
#include "shell/common/node_bindings.h"
#include "shell/common/node_includes.h"
#include "shell/common/node_util.h"
#include "shell/common/path_provider.h"

#if !defined(OFFICIAL_BUILD)
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <shellapi.h>
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "shell/common/mac/main_application_bundle.h"
#endif

const char kProcessType[] = "type";

namespace {

constexpr char kRunAsNodeEnv[] = "LYNXTRON_RUN_AS_NODE";

bool IsRunAsNode() {
  auto env = base::Environment::Create();
  return env->HasVar(kRunAsNodeEnv);
}

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

  if (result->early_return() != 0) {
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
#if BUILDFLAG(IS_MAC)
  base::FilePath exe_path =
      base::CommandLine::ForCurrentProcess()->GetProgram();
  if (exe_path.empty()) {
    base::PathService::Get(base::FILE_EXE, &exe_path);
  }
  base::FilePath snapshot_path =
      exe_path.DirName()
          .DirName()
          .Append("Frameworks")
          .Append(LYNXTRON_PRODUCT_NAME " Framework.framework")
          .Append("Resources")
          .Append("snapshot_blob.bin");
  base::File snapshot_file(snapshot_path,
                           base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (snapshot_file.IsValid()) {
    base::MemoryMappedFile::Region region =
        base::MemoryMappedFile::Region::kWholeFile;
    gin::V8Initializer::LoadV8SnapshotFromFile(
        std::move(snapshot_file), &region, gin::V8SnapshotFileType::kDefault);
  } else {
    gin::V8Initializer::LoadV8Snapshot(gin::V8SnapshotFileType::kDefault);
  }
#else
  gin::V8Initializer::LoadV8Snapshot(gin::V8SnapshotFileType::kDefault);
#endif

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

    node::IsolateData* isolate_data =
        node::CreateIsolateData(isolate, loop, js_env.platform());

    uint64_t env_flags = node::EnvironmentFlags::kDefaultFlags |
                         node::EnvironmentFlags::kHideConsoleWindows;

    node::Environment* env = lynxtron::util::CreateEnvironment(
        isolate, isolate_data, isolate->GetCurrentContext(), result->args(),
        result->exec_args(),
        static_cast<node::EnvironmentFlags::Flags>(env_flags));

    node::SetIsolateUpForNode(isolate);
    node::LoadEnvironment(env, node::StartExecutionCallback{},
                          &lynxtron::OnNodePreload);

    exit_code = node::SpinEventLoop(env).FromMaybe(1);

    node::ResetStdio();
    node::Stop(env, node::StopFlags::kDoNotTerminateIsolate);
    node::FreeEnvironment(env);
    node::FreeIsolateData(isolate_data);
  }

  node::TearDownOncePerProcess();
  base::ThreadPoolInstance::Get()->Shutdown();

  return exit_code;
}

}  // namespace

// TODO(Guo Xi): refer to content/app/content_main_runner_impl.cc for the
// following features:
// 1. ADDRESS_SANITIZER enables ASAN
// 3. InstallConsoleControlHandler
// 4. base::HangWatcher monitors hang events
// 6. Whether we need to call Shutdown, refer to ContentMainRunnerImpl::Shutdown
// 7. ? _CrtDumpMemoryLeaks

#if BUILDFLAG(IS_WIN)
int LynxtronMain() {
#elif BUILDFLAG(IS_MAC)
int LynxtronMain(int argc, char* argv[]) {
#endif
  base::AtExitManager exit_manager;

#if BUILDFLAG(IS_WIN)
  {
    int argc = 0;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (!argv) {
      return -1;
    }
    base::CommandLine::Init(0, nullptr);  // args ignored on Windows
    lynxtron::LynxtronCommandLine::Init(argc, argv);
    LocalFree(argv);
  }
#elif BUILDFLAG(IS_MAC)
  base::CommandLine::Init(argc, argv);
  lynxtron::LynxtronCommandLine::Init(argc, argv);
#endif
  logging::InitElectronLogging(*base::CommandLine::ForCurrentProcess(),
                               /* is_preinit = */ true);

  base::i18n::InitializeICU();

#if !defined(OFFICIAL_BUILD)
  base::debug::EnableInProcessStackDumping();
  base::debug::VerifyDebugger();
#endif  // !defined(OFFICIAL_BUILD)

  if (IsRunAsNode()) {
    return RunNodeMain();
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  constexpr char kRelauncherProcess[] = "relauncher";
  const std::string process_type =
      command_line->GetSwitchValueASCII(kProcessType);
  if (process_type == kRelauncherProcess) {
    return relauncher::RelauncherMain();
  }

  auto runner = lynxtron::MainRunner::Create();
  int exit_code = runner->Initialize();
  exit_code = runner->Run();
  runner->Shutdown();
  return exit_code;
}
