// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/library_main.h"

#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "shell/app/main_runner.h"
#include "shell/app/relauncher.h"
#include "shell/common/logging.h"
#include "shell/common/lynxtron_command_line.h"

#if !defined(OFFICIAL_BUILD)
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <shellapi.h>
#endif

const char kProcessType[] = "type";

// TODO(Guo Xi): refer to content/app/content_main_runner_impl.cc for the
// following features:
// 1. ADDRESS_SANITIZER enables ASAN
// 2. LoadV8SnapshotFile loads V8 snapshot file
// 3. InstallConsoleControlHandler
// 4. base::HangWatcher monitors hang events
// 5. base::PowerMonitor monitors power events
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
  base::CommandLine::Init(0, nullptr);
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
