// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_APP_MAIN_PARTS_H_
#define LYNXTRON_SHELL_APP_MAIN_PARTS_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "shell/app/application.h"
#include "shell/app/main_parts_delegate.h"
#include "ui/display/screen.h"

namespace node {
class Environment;
}

#if BUILDFLAG(IS_WIN)
namespace base {
namespace win {
class ScopedCOMInitializer;
}
}  // namespace base
#endif

namespace lynxtron {
class LynxtronBindings;
class JavascriptEnvironment;
class NodeBindings;
class IconManager;
class GlobalThread;

class MainParts {
 public:
  MainParts();
  ~MainParts();

  static MainParts* Get();

  // Sets the exit code, will fail if the message loop is not ready.
  bool SetExitCode(int code);

  // Gets the exit code
  int GetExitCode() const;

  // Returns handle to the class responsible for extracting file icons.
  IconManager* GetIconManager();

  Application* application() { return application_.get(); }

  void Initialize();
  void WillRunMainMessageLoop(std::unique_ptr<base::RunLoop>& run_loop);
  void PostMainMessageLoopRun();
  void Shutdown();

 private:
#if BUILDFLAG(IS_MAC)
  // Set signal handlers.
  void HandleSIGCHLD();
  void InstallShutdownSignalHandlers(
      base::OnceCallback<void()> shutdown_callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
#endif

  // A place to remember the exit code once the message loop is ready.
  // Before then, we just exit() without any intermediate steps.
  std::optional<int> exit_code_;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif

  std::unique_ptr<GlobalThread> global_thread_;
  std::unique_ptr<JavascriptEnvironment> js_env_;
  std::unique_ptr<Application> application_;

  std::unique_ptr<NodeBindings> node_bindings_;
  // depends-on: js_env_'s isolate
  std::shared_ptr<node::Environment> node_env_;
  std::unique_ptr<LynxtronBindings> lynxtron_bindings_;
  std::unique_ptr<IconManager> icon_manager_;

#if BUILDFLAG(IS_MAC)
  std::unique_ptr<display::ScopedNativeScreen> scoped_native_screen_;
#endif

  static MainParts* self_;

#if BUILDFLAG(IS_MAC)
  void FreeAppDelegate();
  void InitializeMacMainMessageLoop();
  void RegisterURLHandler();
  void InitializeMainNib();
  void RegisterAtomCrApp();
#endif

#if defined(OS_MAC)
  // Todo linshengwei only for test remove later
  //  void TestLynxWindow();
#endif
 private:
  std::unique_ptr<MainPartsDelegate> main_parts_delegate_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_MAIN_PARTS_H_
