// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/library_main.h"

#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/i18n/icu_util.h"
#include "build/buildflag.h"
#if defined(ADDRESS_SANITIZER)
#include "base/debug/asan_service.h"
#endif
#include "shell/app/main_runner.h"
#include "shell/app/relauncher.h"
#include "shell/common/fuses.h"
#include "shell/common/logging.h"
#include "shell/common/lynxtron_command_line.h"

#if !defined(OFFICIAL_BUILD)
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <shellapi.h>

#include "base/functional/bind.h"
#include "shell/app/application.h"
#include "shell/common/global_thread.h"

void HandleConsoleControlEventOnUIThread(DWORD control_type) {
  if (lynxtron::Application::Get()) {
    lynxtron::Application::Get()->Quit();
  }
}

BOOL WINAPI ConsoleControlHandler(DWORD control_type) {
  // Delegate session handling on the main thread and hangs the control thread.
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&HandleConsoleControlEventOnUIThread, control_type));
  return TRUE;
}

void InstallConsoleControlHandler() {
  if (!::SetConsoleCtrlHandler(&ConsoleControlHandler, /*Add=*/TRUE)) {
    DLOG(ERROR) << "Failed to install console control handler";
  }
}
#endif

#if BUILDFLAG(IS_MAC)
#endif

const char kProcessType[] = "type";

#include "shell/app/node_main.h"
#include "shell/common/process_type_registry.h"

namespace {

constexpr char kRunAsNodeEnv[] = "LYNXTRON_RUN_AS_NODE";

bool IsRunAsNode() {
  auto env = base::Environment::Create();
  return lynxtron::fuses::IsRunAsNodeEnabled() && env->HasVar(kRunAsNodeEnv);
}

}  // namespace

#if BUILDFLAG(IS_WIN)
int LynxtronMain() {
#elif BUILDFLAG(IS_MAC)
int LynxtronMain(int argc, char* argv[]) {
#endif
  base::AtExitManager exit_manager;

#if defined(ADDRESS_SANITIZER)
  base::debug::AsanService::GetInstance()->Initialize();
#endif

#if BUILDFLAG(IS_WIN)
  InstallConsoleControlHandler();

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
  lynxtron::InitLogging(*base::CommandLine::ForCurrentProcess(),
                        /* is_preinit = */ true);

  base::i18n::InitializeICU();

#if !defined(OFFICIAL_BUILD)
  base::debug::EnableInProcessStackDumping();
  base::debug::VerifyDebugger();
#endif  // !defined(OFFICIAL_BUILD)

  if (IsRunAsNode()) {
    return lynxtron::RunNodeMain();
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  constexpr char kRelauncherProcess[] = "relauncher";
  const std::string process_type =
      command_line->GetSwitchValueASCII(kProcessType);
  if (process_type == kRelauncherProcess) {
    return relauncher::RelauncherMain();
  }

  if (!process_type.empty()) {
    auto& registry = lynxtron::GetProcessTypeRegistry();
    auto it = registry.find(process_type);
    if (it != registry.end()) {
      return it->second.Run(*command_line);
    }
  }
  auto runner = lynxtron::MainRunner::Create();
  int exit_code = runner->Initialize();
  if (exit_code == 0) {
    exit_code = runner->Run();
  }
  runner->Shutdown();
  return exit_code;
}
