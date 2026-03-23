// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/main_parts.h"

#include <string>
#include <utility>

#include "app/application.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/hang_watcher.h"
#include "build/build_config.h"
#include "gin/v8_initializer.h"
#include "main_parts_delegate.h"
#include "shell/api/lynx_view/lynx_view.h"
#include "shell/api/lynxtron_bindings.h"
#include "shell/app/icon_manager.h"
#include "shell/app/javascript_environment.h"
#include "shell/common/global_thread.h"
#include "shell/common/lynxtron_command_line.h"
#include "shell/common/lynxtron_paths.h"
#include "shell/common/node_bindings.h"
#include "shell/common/node_includes.h"
#include "shell/common/path_provider.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace lynxtron {

namespace {

void InitializeFeatureList() {
  // Initialize base::FeatureList with the command-line flags.
  auto feature_list = std::make_unique<base::FeatureList>();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string enable_features =
      command_line->GetSwitchValueASCII("enable-features");
  std::string disable_features =
      command_line->GetSwitchValueASCII("disable-features");

  feature_list->InitFromCommandLine(enable_features, disable_features);
  base::FeatureList::SetInstance(std::move(feature_list));
}

}  // namespace

// static
MainParts* MainParts::self_ = nullptr;

MainParts::MainParts()
    : application_(std::make_unique<Application>()),
      node_bindings_(NodeBindings::Create()),
      lynxtron_bindings_{
          std::make_unique<LynxtronBindings>(node_bindings_->uv_loop())} {
  DCHECK(!self_) << "Cannot have two MainParts";
  self_ = this;

  // Create MainPartsDelegate if need
  auto& registry = GetGlobalDelegateRegistry();
  auto it = registry.find(kMainPartsDelegateName);
  if (it != registry.end()) {
    auto delegate = it->second.CreateDelegate();
    if (delegate) {
      main_parts_delegate_ = std::unique_ptr<MainPartsDelegate>(
          reinterpret_cast<MainPartsDelegate*>(delegate.release()));
    }
  } else {
    LOG(ERROR) << "MainPartsDelegate not found in registry.";
  }
}

MainParts::~MainParts() = default;

// static
MainParts* MainParts::Get() {
  DCHECK(self_);
  return self_;
}

bool MainParts::SetExitCode(int code) {
  if (!exit_code_) {
    return false;
  }

  *exit_code_ = code;
  return true;
}

int MainParts::GetExitCode() const {
  return exit_code_.value_or(0);
}

void MainParts::Initialize() {
  if (main_parts_delegate_) {
    main_parts_delegate_->PreInitialization();
  }

  base::FeatureList::ClearInstanceForTesting();

#if BUILDFLAG(IS_WIN)
  com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>();
#endif

  InitializeFeatureList();

  base::HangWatcher::InitializeOnMainThread(
      base::HangWatcher::ProcessType::kBrowserProcess,
      /*emit_crashes=*/true);

  if (base::HangWatcher::IsEnabled()) {
    base::HangWatcher::CreateHangWatcherInstance();
    hang_watcher_unregister_thread_closure_ = base::HangWatcher::RegisterThread(
        base::HangWatcher::ThreadType::kMainThread);
    base::HangWatcher::GetInstance()->Start();
  }

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("lynxtron");
#if BUILDFLAG(IS_MAC)
  RegisterAtomCrApp();
  // Initialize native screen for macOS to ensure display::Screen::Get() returns
  // valid screen
  scoped_native_screen_ = std::make_unique<display::ScopedNativeScreen>();
#endif
  // TODO(Guo Xi): path service
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);

  global_thread_ = std::make_unique<GlobalThread>();

  gin::V8Initializer::LoadV8Snapshot(gin::V8SnapshotFileType::kDefault);

  // The ProxyResolverV8 has setup a complete V8 environment, in order to
  // avoid conflicts we only initialize our V8 environment after that.
  js_env_ = std::make_unique<JavascriptEnvironment>(node_bindings_->uv_loop());

  v8::Isolate* const isolate = js_env_->isolate();
  v8::HandleScope scope(isolate);
  if (main_parts_delegate_) {
    main_parts_delegate_->PostV8Initialization();
  }
  node_bindings_->Initialize(isolate, isolate->GetCurrentContext());

  // Create the global environment.
  node_env_ = node_bindings_->CreateEnvironment(
      isolate, isolate->GetCurrentContext(), js_env_->platform(),
      js_env_->max_young_generation_size_in_bytes());

  node_env_->set_trace_sync_io(node_env_->options()->trace_sync_io);

  // We do not want to crash the main process on unhandled rejections.
  node_env_->options()->unhandled_rejections = "warn-with-error-code";

  // Add Electron extended APIs.
  lynxtron_bindings_->BindTo(isolate, node_env_->process_object());

  // Create explicit microtasks runner.
  js_env_->CreateMicrotasksRunner();

  // Wrap the uv loop with global env.
  node_bindings_->set_uv_env(node_env_.get());

  // Load everything.
  node_bindings_->LoadEnvironment(node_env_.get());

  // Wait for app
  node_bindings_->JoinAppCode();

  LynxView::SetNodePlatformEnv(js_env_->platform());

  // #if defined(OS_WIN)
  //   if (!display::Screen::GetScreen()) {
  //     screen_ = views::CreateDesktopScreen();
  //   }
  // #endif

  base::PowerMonitor::GetInstance()->Initialize(
      std::make_unique<base::PowerMonitorDeviceSource>());

  if (main_parts_delegate_) {
    main_parts_delegate_->PostInitialization();
  }

#if BUILDFLAG(IS_MAC)
  InitializeMacMainMessageLoop();
#endif

  node_bindings_->PrepareEmbedThread();
  node_bindings_->StartPolling();

#if !BUILDFLAG(IS_MAC)
  Application::Get()->WillFinishLaunching();
  Application::Get()->DidFinishLaunching(base::Value::Dict());
#endif

  Application::Get()->PreMainMessageLoopRun();
}

void MainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  exit_code_ = 0;
  // js_env_->OnMessageLoopCreated();
  Application::Get()->SetMainMessageLoopQuitClosure(
      run_loop->QuitWhenIdleClosure());
#if BUILDFLAG(IS_MAC)
  InstallShutdownSignalHandlers(
      base::BindOnce(&Application::Quit, base::Unretained(Application::Get())),
      GetUIThreadTaskRunner());
#endif
}

void MainParts::PostMainMessageLoopRun() {
  // Destroy node platform after all destructors_ are executed, as they may
  // invoke Node/V8 APIs inside them.
  node_env_->set_trace_sync_io(false);
  js_env_->DestroyMicrotasksRunner();
  node::Stop(node_env_.get(), node::StopFlags::kDoNotTerminateIsolate);
  node_bindings_->set_uv_env(nullptr);
  node_env_.reset();
}

void MainParts::Shutdown() {
  if (main_parts_delegate_) {
    main_parts_delegate_->PreShutdown();
  }
  global_thread_.reset();
  base::ThreadPoolInstance::Get()->Shutdown();

#if BUILDFLAG(IS_WIN)
  com_initializer_.reset();
#endif
}

IconManager* MainParts::GetIconManager() {
  if (!icon_manager_) {
    icon_manager_ = std::make_unique<IconManager>();
  }
  return icon_manager_.get();
}

}  // namespace lynxtron
