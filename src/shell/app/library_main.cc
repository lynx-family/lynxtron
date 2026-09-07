// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/library_main.h"

#include <stdio.h>

#include <memory>
#include <optional>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/path_service.h"
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
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "shell/common/mac/main_application_bundle.h"
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
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
int LynxtronMain(int argc, char* argv[]) {
#endif
#if BUILDFLAG(IS_MAC)
  // Align the base bundle id used by //base with the real app bundle id.
  // The default fallback is "org.chromium.Chromium", which causes the Mach
  // port rendezvous service to be registered as
  // "org.chromium.Chromium.MachPortRendezvousServer.<pid>". Under the App
  // Sandbox (TestFlight / MAS), launchd rejects that name with
  // "Operation not permitted" because it does not match the app bundle id.
  // This must run before any //base code that touches Mach ports.
  base::apple::SetBaseBundleIDOverride("com.lynxjs.Lynxtron");
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
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  base::CommandLine::Init(argc, argv);
  lynxtron::LynxtronCommandLine::Init(argc, argv);
#endif
  lynxtron::InitLogging(*base::CommandLine::ForCurrentProcess(),
                        /* is_preinit = */ true);

#if BUILDFLAG(IS_MAC)
  base::apple::SetOverrideFrameworkBundlePath(
      lynxtron::MainApplicationBundlePath()
          .Append("Contents")
          .Append("Frameworks")
          .Append(LYNXTRON_PRODUCT_NAME " Framework.framework"));
  base::apple::SetOverrideOuterBundlePath(
      lynxtron::MainApplicationBundlePath());
#endif

#if BUILDFLAG(IS_HARMONY)
  // A HAP has no console: the ArkTS app process closes stdout/stderr, so
  // everything V8 and Node write straight to those descriptors — V8_Fatal
  // messages, uncaught JS exceptions during bootstrap — is discarded, and a
  // fatal error shows up as nothing but a SIGTRAP in the system crash log.
  // Chromium's own logging goes to --log-file and is unaffected; this only
  // rescues the two runtimes that bypass it.
  {
    auto env = base::Environment::Create();
    std::optional<std::string> files_dir = env->GetVar("LYNXTRON_FILES_DIR");
    if (files_dir && !files_dir->empty()) {
      const std::string stdio_log = *files_dir + "/lynxtron_stdio.log";
      if (freopen(stdio_log.c_str(), "w", stderr)) {
        setvbuf(stderr, nullptr, _IONBF, 0);
        if (freopen(stdio_log.c_str(), "a", stdout)) {
          setvbuf(stdout, nullptr, _IONBF, 0);
        }
      }
    }
  }

  // Point DIR_ASSETS at the HAP's resfile directory before anything reads it.
  //
  // On other platforms DIR_ASSETS derives from DIR_EXE, but a HAP's native
  // code runs inside the ArkTS app process, so /proc/self/exe names the
  // runtime host rather than anything of ours and the staged runtime files —
  // icudtl.dat, snapshot_blob.bin, resources/default_app.asar,
  // resources/lynx_core.js — sit somewhere else entirely.  Without this,
  // InitializeICU() aborts the process on "Invalid file descriptor to ICU
  // data received" before the first window is ever created.
  //
  // The ETS bridge publishes the ability context's resourceDir as
  // LYNXTRON_EXE_PATH, which is authoritative: it already carries whatever
  // module name the HAP was packaged under.  Fall back to the conventional
  // mount points only for processes started without an ability context.
  {
    base::FilePath assets;
    auto env = base::Environment::Create();
    std::optional<std::string> resource_dir = env->GetVar("LYNXTRON_EXE_PATH");
    if (resource_dir && !resource_dir->empty()) {
      assets = base::FilePath(*resource_dir);
    } else {
      static constexpr const char* kCandidateAssetDirs[] = {
          "/data/storage/el1/bundle/entry/resources/resfile",
          "/data/storage/el1/bundle/resources/resfile",
          "/data/storage/el2/base/resources/resfile",
      };
      for (const char* candidate : kCandidateAssetDirs) {
        base::FilePath dir(candidate);
        if (base::PathExists(dir.AppendASCII("icudtl.dat"))) {
          assets = dir;
          break;
        }
      }
    }
    if (assets.empty() ||
        !base::PathService::Override(base::DIR_ASSETS, assets)) {
      LOG(ERROR) << "Could not locate the HAP resfile directory; ICU and the "
                    "Node.js bootstrap will fail to load their data files.";
    }
  }
#endif  // BUILDFLAG(IS_HARMONY)

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
