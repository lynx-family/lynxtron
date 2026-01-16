// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/app/main_parts.h"

#include <memory>
#include <string>

#include "app/application.h"
#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "gin/v8_initializer.h"
#include "shell/api/lynxtron_bindings.h"
#include "shell/app/javascript_environment.h"
#include "shell/common/global_thread.h"
#include "shell/common/lynxtron_paths.h"
#include "shell/common/node_bindings.h"
#include "shell/common/node_includes.h"
#include "shell/common/path_provider.h"
#include "shell/lynx_view_holder/lynx_view.h"

namespace lynxtron {

// static
MainParts* MainParts::self_ = nullptr;

MainParts::MainParts()
    : application_(std::make_unique<Application>()),
      node_bindings_(NodeBindings::Create()),
      lynxtron_bindings_{
          std::make_unique<LynxtronBindings>(node_bindings_->uv_loop())} {
  DCHECK(!self_) << "Cannot have two MainParts";
  self_ = this;
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
  base::FeatureList::ClearInstanceForTesting();

  // TODO(Guo Xi): initialize feature list
  // InitializeFeatureList();

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("lynxtron");
#if BUILDFLAG(IS_MAC)
  RegisterAtomCrApp();
  // Initialize native screen for macOS to ensure display::Screen::Get() returns
  // valid screen
  scoped_native_screen_ = std::make_unique<display::ScopedNativeScreen>();
#endif
  // TODO(Guo Xi): path service
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);

  GlobalThread::Create();

  // TODO(Guo Xi): confirm the implementation of LoadV8Snapshot in electron
  // Reference:
  // https://source.chromium.org/chromium/chromium/src/+/main:content/app/content_main_runner_impl.cc;bpv=1;bpt=1
  gin::V8Initializer::LoadV8Snapshot(gin::V8SnapshotFileType::kDefault);

  // The ProxyResolverV8 has setup a complete V8 environment, in order to
  // avoid conflicts we only initialize our V8 environment after that.
  js_env_ = std::make_unique<JavascriptEnvironment>(node_bindings_->uv_loop());

  v8::Isolate* const isolate = js_env_->isolate();
  v8::HandleScope scope(isolate);
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

  // std::vector<ui::ResourceScaleFactor> supported_scale_factors;
  // // On platforms other than iOS, 100P is always a supported scale factor.
  // supported_scale_factors.push_back(ui::k100Percent);
  // supported_scale_factors.push_back(ui::k200Percent);
  // SetSupportedResourceScaleFactors(supported_scale_factors);

  // Add Electron extended APIs.
  // electron_bindings_->BindTo(js_env_->isolate(), env->process_object());

  // Load everything.
  // node_bindings_->LoadEnvironment(env);
  // Wrap the uv loop with global env.
  // node_bindings_->set_uv_env(env);

  // We already initialized the feature list in PreEarlyInitialization(), but
  // the user JS script would not have had a chance to alter the command-line
  // switches at that point. Lets reinitialize it here to pick up the
  // command-line changes.

  // Initialize field trials.
  // InitializeFieldTrials();

  // Reinitialize logging now that the app has had a chance to set the app name
  // and/or user data directory.
  // logging::InitElectronLogging(*base::CommandLine::ForCurrentProcess(),
  //                              /* is_preinit = */ false);

  // Initialize after user script environment creation.
  // fake_application_process_->PostEarlyInitialization();

  // mojo::core::Configuration mojo_config;
  // mojo_config.is_broker_process = false;
  // mojo::core::Init(mojo_config);
  // mojo_ipc_thread_.StartWithOptions(
  //     base::Thread::Options(base::MessagePumpType::IO, 0));
  // scoped_refptr<base::SingleThreadTaskRunner> mojo_ipc_task_runner =
  //     mojo_ipc_thread_.task_runner();
  // mojo_ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
  //     mojo_ipc_task_runner,
  //     mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  // base::PowerMonitor::Initialize(
  //     std::make_unique<base::PowerMonitorDeviceSource>());
}

int MainParts::PreMainMessageLoopRun() {
  Application::Get()->PreCreateThreads();

  // Run user's main script before most things get initialized, so we can have
  // a chance to setup everything.
  node_bindings_->PrepareEmbedThread();
  node_bindings_->StartPolling();

  // TODO(Guo Xi): new electron code has this, whether we need it
  // url::Add*Scheme are not threadsafe, this helps prevent data races.
  // url::LockSchemeRegistries();

#if !BUILDFLAG(IS_MAC)
  // The corresponding call in macOS is in LynxtronApplicationDelegate.
  Application::Get()->WillFinishLaunching();
  Application::Get()->DidFinishLaunching(base::Value::Dict());
#endif
  // Notify observers that main thread message loop was initialized.
  Application::Get()->PreMainMessageLoopRun();

#if defined(OS_MAC)
  // Todo linshengwei only for test remove later
  //  TestLynxWindow();
#endif

  return 0;
}

void MainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  exit_code_ = 0;
  // js_env_->OnMessageLoopCreated();
  Application::Get()->SetMainMessageLoopQuitClosure(
      run_loop->QuitWhenIdleClosure());
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

#if BUILDFLAG(IS_WIN)
void MainParts::PreCreateMainMessageLoop() {
  PreCreateMainMessageLoopCommon();
}
#endif

#if BUILDFLAG(IS_MAC)
void MainParts::PostCreateMainMessageLoop() {
  // Exit in response to SIGINT, SIGTERM, etc.
  InstallShutdownSignalHandlers(
      base::BindOnce(&Application::Quit, base::Unretained(Application::Get())),
      GetUIThreadTaskRunner());
}
#endif

void MainParts::Shutdown() {
  // content::BrowserTaskExecutor::Shutdown();
  base::ThreadPoolInstance::Get()->Shutdown();
}

void MainParts::PreCreateMainMessageLoopCommon() {
#if BUILDFLAG(IS_MAC)
  InitializeMainNib();
  RegisterURLHandler();
#endif
}

// IconManager* MainParts::GetIconManager() {
//   // DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
//   if (!icon_manager_.get())
//     icon_manager_ = std::make_unique<IconManager>();
//   return icon_manager_.get();
// }

}  // namespace lynxtron
