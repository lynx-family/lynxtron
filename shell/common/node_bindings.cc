// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/common/node_bindings.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/allocator/partition_allocator/src/partition_alloc/oom.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
// #include "chrome/common/chrome_version.h"
// #include "content/public/common/content_paths.h"
// #include "electron/buildflags/buildflags.h"
#include "lynxtron/lynxtron_version.h"
// #include "electron/fuses.h"
// #include "electron/mas.h"
// #include "shell/browser/api/api_app.h"
// #include "shell/common/api/electron_bindings.h"
#include "gin/function_template.h"
#include "shell/app/application.h"
#include "shell/common/gin_converters/callback_converter.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/event.h"
#include "shell/common/gin_helper/event_emitter_caller.h"
#include "shell/common/gin_helper/handle.h"
#include "shell/common/lynxtron_command_line.h"
#include "shell/common/mac/main_application_bundle.h"
#include "shell/common/node_includes.h"
#include "shell/common/node_util.h"
#include "third_party/node/src/debug_utils.h"
#include "third_party/node/src/module_wrap.h"

#define LYNXTRON_BROWSER_BINDINGS(V) \
  V(lynxtron_binding_app)            \
  V(lynxtron_binding_v8_util)        \
  V(lynxtron_binding_asar)           \
  V(lynxtron_binding_command_line)   \
  V(lynxtron_binding_environment)    \
  V(lynxtron_binding_menu)           \
  V(lynxtron_binding_shell)          \
  V(lynxtron_binding_tray)           \
  V(lynxtron_binding_event_emitter)  \
  V(lynxtron_base_window)            \
  V(lynxtron_lynx_window)            \
  V(lynxtron_binding_native_image)   \
  V(lynxtron_binding_screen)         \
  V(lynxtron_binding_dialog)

#define ELECTRON_TESTING_BINDINGS(V) V(lynxtron_binding_testing)

// This is used to load built-in bindings. Instead of using
// __attribute__((constructor)), we call the _register_<modname>
// function for each built-in bindings explicitly. This is only
// forward declaration. The definitions are in each binding's
// implementation when calling the NODE_LINKED_BINDING_CONTEXT_AWARE.
#define V(modname) void _register_##modname();
LYNXTRON_BROWSER_BINDINGS(V)
// ELECTRON_COMMON_BINDINGS(V)
// ELECTRON_RENDERER_BINDINGS(V)
// ELECTRON_UTILITY_BINDINGS(V)
// #if DCHECK_IS_ON()
// ELECTRON_TESTING_BINDINGS(V)
// #endif
#undef V

using node::loader::ModuleWrap;

namespace {
bool g_is_initialized = false;

void V8FatalErrorCallback(const char* location, const char* message) {
  LOG(ERROR) << "Fatal error in V8: " << location << " " << message;

  // #if !IS_MAS_BUILD()
  //   electron::crash_keys::SetCrashKey("electron.v8-fatal.message", message);
  //   electron::crash_keys::SetCrashKey("electron.v8-fatal.location",
  //   location);
  // #endif

  volatile int* zero = nullptr;
  *zero = 0;
}

void V8OOMErrorCallback(const char* location, const v8::OOMDetails& details) {
  const char* message =
      details.is_heap_oom ? "Allocation failed - JavaScript heap out of memory"
                          : "Allocation failed - process out of memory";
  if (location) {
    LOG(ERROR) << "OOM error in V8: " << location << " " << message;
  } else {
    LOG(ERROR) << "OOM error in V8: " << message;
  }
  if (details.detail) {
    LOG(ERROR) << "OOM detail: " << details.detail;
  }

  // #if !IS_MAS_BUILD()
  //   electron::crash_keys::SetCrashKey("electron.v8-oom.is_heap_oom",
  //                                     std::to_string(details.is_heap_oom));
  //   if (location) {
  //     electron::crash_keys::SetCrashKey("electron.v8-oom.location",
  //     location);
  //   }
  //   if (details.detail) {
  //     electron::crash_keys::SetCrashKey("electron.v8-oom.detail",
  //     details.detail);
  //   }
  // #endif

  OOM_CRASH(0);
}

bool AllowWasmCodeGenerationCallback(v8::Local<v8::Context> context,
                                     v8::Local<v8::String> source) {
  return node::AllowWasmCodeGenerationCallback(context, source);
}

v8::MaybeLocal<v8::Promise> HostImportModuleWithPhaseDynamically(
    v8::Local<v8::Context> context,
    v8::Local<v8::Data> v8_host_defined_options,
    v8::Local<v8::Value> v8_referrer_resource_url,
    v8::Local<v8::String> v8_specifier,
    v8::ModuleImportPhase import_phase,
    v8::Local<v8::FixedArray> v8_import_attributes) {
  if (node::Environment::GetCurrent(context) == nullptr) {
    return {};
  }

  // TODO: Switch to node::loader::ImportModuleDynamicallyWithPhase
  // once we land the Node.js version that has it in upstream.
  CHECK(import_phase == v8::ModuleImportPhase::kEvaluation);
  return node::loader::ImportModuleDynamically(
      context, v8_host_defined_options, v8_referrer_resource_url, v8_specifier,
      v8_import_attributes);
}

v8::MaybeLocal<v8::Promise> HostImportModuleDynamically(
    v8::Local<v8::Context> context,
    v8::Local<v8::Data> v8_host_defined_options,
    v8::Local<v8::Value> v8_referrer_resource_url,
    v8::Local<v8::String> v8_specifier,
    v8::Local<v8::FixedArray> v8_import_attributes) {
  return HostImportModuleWithPhaseDynamically(
      context, v8_host_defined_options, v8_referrer_resource_url, v8_specifier,
      v8::ModuleImportPhase::kEvaluation, v8_import_attributes);
}

void HostInitializeImportMetaObject(v8::Local<v8::Context> context,
                                    v8::Local<v8::Module> module,
                                    v8::Local<v8::Object> meta) {
  node::Environment* env = node::Environment::GetCurrent(context);
  if (env == nullptr) {
    return;
  }

  return ModuleWrap::HostInitializeImportMetaObjectCallback(context, module,
                                                            meta);
}

v8::ModifyCodeGenerationFromStringsResult ModifyCodeGenerationFromStrings(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> source,
    bool is_code_like) {
  return node::ModifyCodeGenerationFromStrings(context, source, is_code_like);
}

// void ErrorMessageListener(v8::Local<v8::Message> message,
//                           v8::Local<v8::Value> data) {
//   v8::Isolate* isolate = v8::Isolate::GetCurrent();
//   node::Environment* env = node::Environment::GetCurrent(isolate);
//   if (env) {
//     v8::MicrotasksScope microtasks_scope(
//         env->context(), v8::MicrotasksScope::kDoNotRunMicrotasks);
//     // Emit the after() hooks now that the exception has been handled.
//     // Analogous to node/lib/internal/process/execution.js#L176-L180
//     if (env->async_hooks()->fields()[node::AsyncHooks::kAfter]) {
//       while (env->async_hooks()->fields()[node::AsyncHooks::kStackLength]) {
//         double id = env->execution_async_id();
//         // Do not call EmitAfter for asyncId 0.
//         if (id != 0) {
//           node::AsyncWrap::EmitAfter(env, id);
//         }
//         env->async_hooks()->pop_async_context(id);
//       }
//     }

//     // Ensure that the async id stack is properly cleared so the async
//     // hook stack does not become corrupted.
//     env->async_hooks()->clear_async_id_stack();
//   }
// }

// Only allow a specific subset of options in non-LYNXTRON_RUN_AS_NODE mode.
// If node CLI inspect support is disabled, allow no debug options.
bool IsAllowedOption(const std::string_view option) {
  static constexpr auto debug_options =
      base::MakeFixedFlatSet<std::string_view>({
          "--debug",
          "--debug-brk",
          "--debug-port",
          "--inspect",
          "--inspect-brk",
          "--inspect-brk-node",
          "--inspect-port",
          "--inspect-publish-uid",
          "--experimental-network-inspection",
      });

  // This should be aligned with what's possible to set via the process object.
  static constexpr auto options = base::MakeFixedFlatSet<std::string_view>({
      "--diagnostic-dir",
      "--dns-result-order",
      "--no-deprecation",
      "--throw-deprecation",
      "--trace-deprecation",
      "--trace-warnings",
      "--no-experimental-global-navigator",
  });

  if (debug_options.contains(option)) {
    // TODO(Guo Xi): fuse
    return true;
    // return electron::fuses::IsNodeCliInspectEnabled();
  }

  return options.contains(option);
}

// Initialize NODE_OPTIONS to pass to Node.js
// See https://nodejs.org/api/cli.html#cli_node_options_options
void SetNodeOptions(base::Environment* env) {
  // Options that are expressly disallowed
  // static constexpr auto disallowed =
  // base::MakeFixedFlatSet<std::string_view>({
  //     "--enable-fips",
  //     "--experimental-policy",
  //     "--force-fips",
  //     "--openssl-config",
  //     "--use-bundled-ca",
  //     "--use-openssl-ca",
  // });

  // static constexpr auto pkg_opts = base::MakeFixedFlatSet<std::string_view>({
  //     "--http-parser",
  //     "--max-http-header-size",
  // });

  // if (env->HasVar("NODE_EXTRA_CA_CERTS")) {
  //   if (!electron::fuses::IsNodeOptionsEnabled()) {
  //     LOG(WARNING) << "NODE_OPTIONS ignored due to disabled nodeOptions
  //     fuse."; env->UnSetVar("NODE_EXTRA_CA_CERTS");
  //   }
  // }

  // TODO(Guo Xi): NODE_OPTIONS
  // if (env->HasVar("NODE_OPTIONS")) {
  //   if (electron::fuses::IsNodeOptionsEnabled()) {
  //     std::string result_options;
  //     std::string options = env->GetVar("NODE_OPTIONS").value();
  //     const std::vector<std::string_view> parts = base::SplitStringPiece(
  //         options, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  //     bool is_packaged_app = electron::api::App::IsPackaged();

  //     for (const std::string_view part : parts) {
  //       // Strip off values passed to individual NODE_OPTIONs
  //       const std::string_view option = part.substr(0, part.find('='));

  //       if (is_packaged_app && !pkg_opts.contains(option)) {
  //         // Explicitly disallow majority of NODE_OPTIONS in packaged apps
  //         LOG(ERROR) << "Most NODE_OPTIONs are not supported in packaged
  //         apps."
  //                    << " See documentation for more details.";
  //         continue;
  //       } else if (disallowed.contains(option)) {
  //         // Remove NODE_OPTIONS specifically disallowed for use in Node.js
  //         // through Electron owing to constraints like BoringSSL.
  //         LOG(ERROR) << "The NODE_OPTION " << option
  //                    << " is not supported in Electron";
  //         continue;
  //       }
  //       result_options.append(part);
  //       result_options.append(" ");
  //     }

  //     // overwrite new NODE_OPTIONS without unsupported variables
  //     env->SetVar("NODE_OPTIONS", result_options);
  //   } else {
  //     LOG(WARNING) << "NODE_OPTIONS ignored due to disabled nodeOptions
  //     fuse."; env->UnSetVar("NODE_OPTIONS");
  //   }
  // }
}

}  // namespace

namespace lynxtron {

namespace {

base::FilePath GetResourcesPath() {
#if BUILDFLAG(IS_MAC)
  return lynxtron::MainApplicationBundlePath()
      .Append("Contents")
      .Append("Resources");
#else
  base::FilePath assets_path;
  base::PathService::Get(base::DIR_ASSETS, &assets_path);

  return assets_path.Append(FILE_PATH_LITERAL("resources"));
#endif
}
}  // namespace

NodeBindings::NodeBindings() : uv_loop_{InitEventLoop(&worker_loop_)} {}

NodeBindings::~NodeBindings() {
  // Quit the embed thread.
  embed_closed_ = true;
  uv_sem_post(&embed_sem_);

  WakeupEmbedThread();

  // Wait for everything to be done.
  uv_thread_join(&embed_thread_);

  // Clear uv.
  uv_sem_destroy(&embed_sem_);
  dummy_uv_handle_.reset();
}

node::IsolateData* NodeBindings::isolate_data(
    v8::Local<v8::Context> context) const {
  if (context->GetNumberOfEmbedderDataFields() <=
      kElectronContextEmbedderDataIndex) {
    return nullptr;
  }
  auto* isolate_data = static_cast<node::IsolateData*>(
      context->GetAlignedPointerFromEmbedderData(
          kElectronContextEmbedderDataIndex));
  CHECK(isolate_data);
  CHECK(isolate_data->event_loop());
  return isolate_data;
}

// static
uv_loop_t* NodeBindings::InitEventLoop(uv_loop_t* worker_loop) {
  uv_loop_t* event_loop = nullptr;

  event_loop = uv_default_loop();

  // Interrupt embed polling when a handle is started.
  uv_loop_configure(event_loop, UV_LOOP_INTERRUPT_ON_IO_CHANGE);

  return event_loop;
}

void NodeBindings::RegisterBuiltinBindings() {
#define V(modname) _register_##modname();
  LYNXTRON_BROWSER_BINDINGS(V)
  // ELECTRON_COMMON_BINDINGS(V)

// #if DCHECK_IS_ON()
//   ELECTRON_TESTING_BINDINGS(V)
// #endif
#undef V
}

bool NodeBindings::IsInitialized() {
  return g_is_initialized;
}

// Initialize Node.js cli options to pass to Node.js
// See https://nodejs.org/api/cli.html#cli_options
std::vector<std::string> NodeBindings::ParseNodeCliFlags() {
  const auto argv = base::CommandLine::ForCurrentProcess()->argv();
  std::vector<std::string> args;

  // TODO(codebytere): We need to set the first entry in args to the
  // process name owing to src/node_options-inl.h#L286-L290 but this is
  // redundant and so should be refactored upstream.
  args.reserve(argv.size() + 1);

  // TODO(Guo Xi): electron
  args.emplace_back("electron");

  for (const auto& arg : argv) {
#if BUILDFLAG(IS_WIN)
    const auto& option = base::WideToUTF8(arg);
#else
    const auto& option = arg;
#endif
    const auto stripped = std::string_view{option}.substr(0, option.find('='));
    // Only allow no-op or a small set of debug/trace related options.
    if (IsAllowedOption(stripped) || stripped == "--") {
      args.push_back(option);
    }
  }

  return args;
}

void NodeBindings::Initialize(v8::Isolate* const isolate,
                              v8::Local<v8::Context> context) {
  TRACE_EVENT0("electron", "NodeBindings::Initialize");
  // Open node's error reporting system for browser process.

  // Explicitly register electron's builtin bindings.
  RegisterBuiltinBindings();

  auto env = base::Environment::Create();
  SetNodeOptions(env.get());

  // Parse and set Node.js cli flags.
  std::vector<std::string> args = ParseNodeCliFlags();

  // V8::EnableWebAssemblyTrapHandler can be called only once or it will
  // hard crash. We need to prevent Node.js calling it in the event it has
  // already been called.
  node::per_process::cli_options->disable_wasm_trap_handler = true;

  uint64_t process_flags =
      node::ProcessInitializationFlags::kNoInitializeV8 |
      node::ProcessInitializationFlags::kNoInitializeNodeV8Platform |
      node::ProcessInitializationFlags::kEnableStdioInheritance;

  // TODO(Guo Xi): fuses::IsNodeOptionsEnabled
  // if (!fuses::IsNodeOptionsEnabled()) {
  //   process_flags |=
  //   node::ProcessInitializationFlags::kDisableNodeOptionsEnv;
  // }

  std::shared_ptr<node::InitializationResult> result =
      node::InitializeOncePerProcess(
          args,
          static_cast<node::ProcessInitializationFlags::Flags>(process_flags));

  for (const std::string& error : result->errors()) {
    std::cerr << args[0] << ": " << error << '\n';
  }

  if (result->early_return() != 0) {
    exit(result->exit_code());
  }

#if BUILDFLAG(IS_WIN)
  // uv_init overrides error mode to suppress the default crash dialog, bring
  // it back if user wants to show it.
  if (env->HasVar("ELECTRON_DEFAULT_ERROR_MODE")) {
    SetErrorMode(GetErrorMode() & ~SEM_NOGPFAULTERRORBOX);
  }
#endif

  gin_helper::internal::Event::GetConstructor(isolate, context);

  g_is_initialized = true;
}

std::shared_ptr<node::Environment> NodeBindings::CreateEnvironment(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    node::MultiIsolatePlatform* platform,
    size_t max_young_generation_size,
    std::vector<std::string> args,
    std::vector<std::string> exec_args,
    std::optional<base::RepeatingCallback<void()>> on_app_code_ready) {
  // Feed node the path to initialization script.

  gin_helper::Dictionary global(isolate, context->Global());

  const std::vector<std::string> search_paths = {"app.asar", "app",
                                                 "default_app.asar"};
  const std::vector<std::string> app_asar_search_paths = {"app.asar"};
  context->Global()->SetPrivate(
      context,
      v8::Private::ForApi(
          isolate,
          gin::ConvertToV8(isolate, "appSearchPaths").As<v8::String>()),
      // TODO(Guo Xi)
      // gin::ConvertToV8(isolate,
      // electron::fuses::IsOnlyLoadAppFromAsarEnabled()
      //                               ? app_asar_search_paths
      //                               : search_paths));

      gin::ConvertToV8(isolate, search_paths));
  context->Global()->SetPrivate(
      context,
      v8::Private::ForApi(
          isolate, gin::ConvertToV8(isolate, "appSearchPathsOnlyLoadASAR")
                       .As<v8::String>()),
      // TODO(Guo Xi) : electron::fuses::IsOnlyLoadAppFromAsarEnabled()
      // gin::ConvertToV8(isolate,
      //                  electron::fuses::IsOnlyLoadAppFromAsarEnabled()));
      gin::ConvertToV8(isolate, false));

  // TODO(Guo Xi): don't need process_type
  // std::string init_script = "electron/js2c/" + process_type + "_init";
  std::string init_script = "lynxtron/js2c/browser_init";

  args.insert(args.begin() + 1, init_script);

  auto* isolate_data = node::CreateIsolateData(isolate, uv_loop_, platform);
  isolate_data->max_young_gen_size = max_young_generation_size;
  context->SetAlignedPointerInEmbedderData(kElectronContextEmbedderDataIndex,
                                           static_cast<void*>(isolate_data));

  uint64_t env_flags = node::EnvironmentFlags::kDefaultFlags |
                       node::EnvironmentFlags::kHideConsoleWindows |
                       node::EnvironmentFlags::kNoGlobalSearchPaths |
                       node::EnvironmentFlags::kNoRegisterESMLoader;

  // TODO(Guo Xi): fuse
  // if (!electron::fuses::IsNodeCliInspectEnabled()) {
  //   // If --inspect and friends are disabled we also shouldn't listen for
  //   // SIGUSR1
  //   env_flags |= node::EnvironmentFlags::kNoStartDebugSignalHandler;
  // }

  node::Environment* env = util::CreateEnvironment(
      isolate, static_cast<node::IsolateData*>(isolate_data), context, args,
      exec_args, static_cast<node::EnvironmentFlags::Flags>(env_flags));
  DCHECK(env);

  node::IsolateSettings is;

  // Use a custom fatal error callback to allow us to add
  // crash message and location to CrashReports.
  is.fatal_error_callback = V8FatalErrorCallback;
  is.oom_error_callback = V8OOMErrorCallback;

  // We don't want to abort either in the renderer or browser processes.
  // We already listen for uncaught exceptions and handle them there.
  // For utility process we expect the process to behave as standard
  // Node.js runtime and abort the process with appropriate exit
  // code depending on a handler being set for `uncaughtException` event.
  // if (browser_env_ != BrowserEnvironment::kUtility) {
  is.should_abort_on_uncaught_exception_callback = [](v8::Isolate*) {
    return false;
  };
  // }

  // Use a custom callback here to allow us to leverage Blink's logic in the
  // renderer process.
  is.allow_wasm_code_generation_callback = AllowWasmCodeGenerationCallback;
  is.flags |= node::IsolateSettingsFlags::
      ALLOW_MODIFY_CODE_GENERATION_FROM_STRINGS_CALLBACK;
  is.modify_code_generation_from_strings_callback =
      ModifyCodeGenerationFromStrings;
  is.policy = v8::MicrotasksPolicy::kExplicit;

  node::SetIsolateUpForNode(isolate, is);
  isolate->SetHostImportModuleDynamicallyCallback(HostImportModuleDynamically);
  isolate->SetHostImportModuleWithPhaseDynamicallyCallback(
      HostImportModuleWithPhaseDynamically);
  isolate->SetHostInitializeImportMetaObjectCallback(
      HostInitializeImportMetaObject);

  gin_helper::Dictionary process(isolate, env->process_object());
  // process.SetReadOnly("type", process_type);

  if (on_app_code_ready) {
    process.SetMethod("appCodeLoaded", std::move(*on_app_code_ready));
  } else {
    process.SetMethod("appCodeLoaded",
                      base::BindRepeating(&NodeBindings::SetAppCodeLoaded,
                                          base::Unretained(this)));
  }

  auto env_deleter = [isolate, isolate_data,
                      context = v8::Global<v8::Context>{isolate, context}](
                         node::Environment* nenv) mutable {
    // When `isolate_data` was created above, a pointer to it was kept
    // in context's embedder_data[kElectronContextEmbedderDataIndex].
    // Since we're about to free `isolate_data`, clear that entry
    v8::HandleScope handle_scope{isolate};
    context.Get(isolate)->SetAlignedPointerInEmbedderData(
        kElectronContextEmbedderDataIndex, nullptr);
    context.Reset();

    node::FreeEnvironment(nenv);
    node::FreeIsolateData(isolate_data);
  };

  return {env, std::move(env_deleter)};
}

std::shared_ptr<node::Environment> NodeBindings::CreateEnvironment(
    v8::Isolate* const isolate,
    v8::Local<v8::Context> context,
    node::MultiIsolatePlatform* platform,
    size_t max_young_generation_size,
    std::optional<base::RepeatingCallback<void()>> on_app_code_ready) {
  return CreateEnvironment(
      isolate, context, platform, max_young_generation_size,
      lynxtron::LynxtronCommandLine::AsUtf8(), {}, on_app_code_ready);
}

void NodeBindings::LoadEnvironment(node::Environment* env) {
  node::LoadEnvironment(env, node::StartExecutionCallback{}, &OnNodePreload);
  gin_helper::EmitEvent(env->isolate(), env->process_object(), "loaded");
}

void NodeBindings::PrepareEmbedThread() {
  // IOCP does not change for the process until the loop is recreated,
  // we ensure that there is only a single polling thread satisfying
  // the concurrency limit set from CreateIoCompletionPort call by
  // uv_loop_init for the lifetime of this process.
  // More background can be found at:
  // https://github.com/microsoft/vscode/issues/142786#issuecomment-1061673400
  if (initialized_) {
    return;
  }

  // Add dummy handle for libuv, otherwise libuv would quit when there is
  // nothing to do.
  uv_async_init(uv_loop_, dummy_uv_handle_.get(), nullptr);

  // Start worker that will interrupt main loop when having uv events.
  uv_sem_init(&embed_sem_, 0);
  uv_thread_create(&embed_thread_, EmbedThreadRunner, this);
}

void NodeBindings::StartPolling() {
  // Avoid calling UvRunOnce if the loop is already active,
  // otherwise it can lead to situations were the number of active
  // threads processing on IOCP is greater than the concurrency limit.
  if (initialized_) {
    return;
  }

  initialized_ = true;

  // The MessageLoop should have been created, remember the one in main thread.
  task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

  // Run uv loop for once to give the uv__io_poll a chance to add all events.
  UvRunOnce();
}

void NodeBindings::SetAppCodeLoaded() {
  app_code_loaded_ = true;
}

void NodeBindings::JoinAppCode() {
  auto* application = Application::Get();
  node::Environment* env = uv_env();

  if (!env) {
    return;
  }

  v8::HandleScope handle_scope(env->isolate());
  // Enter node context while dealing with uv events.
  v8::Context::Scope context_scope(env->context());

  // Pump the event loop until we get the signal that the app code has finished
  // loading
  while (!app_code_loaded_ && !application->is_shutting_down()) {
    int r = uv_run(uv_loop_, UV_RUN_ONCE);
    if (r == 0) {
      base::RunLoop().QuitWhenIdle();  // Quit from uv.
      break;
    }
  }
}

void NodeBindings::UvRunOnce() {
  node::Environment* env = uv_env();

  // When doing navigation without restarting renderer process, it may happen
  // that the node environment is destroyed but the message loop is still there.
  // In this case we should not run uv loop.
  if (!env) {
    return;
  }

  v8::HandleScope handle_scope(env->isolate());

  // Enter node context while dealing with uv events.
  v8::Context::Scope context_scope(env->context());

  {
    util::ExplicitMicrotasksScope microtasks_scope(
        env->context()->GetMicrotaskQueue());

    // Deal with uv events.
    int r = uv_run(uv_loop_, UV_RUN_NOWAIT);

    if (r == 0) {
      base::RunLoop().QuitWhenIdle();  // Quit from uv.
    }
  }

  // Tell the worker thread to continue polling.
  uv_sem_post(&embed_sem_);
}

void NodeBindings::WakeupMainThread() {
  DCHECK(task_runner_);
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&NodeBindings::UvRunOnce,
                                                   weak_factory_.GetWeakPtr()));
}

void NodeBindings::WakeupEmbedThread() {
  uv_async_send(dummy_uv_handle_.get());
}

// static
void NodeBindings::EmbedThreadRunner(void* arg) {
  auto* self = static_cast<NodeBindings*>(arg);

  while (true) {
    // Wait for the main loop to deal with events.
    uv_sem_wait(&self->embed_sem_);
    if (self->embed_closed_) {
      break;
    }

    // Wait for something to happen in uv loop.
    // Note that the PollEvents() is implemented by derived classes, so when
    // this class is being destructed the PollEvents() would not be available
    // anymore. Because of it we must make sure we only invoke PollEvents()
    // when this class is alive.
    self->PollEvents();
    if (self->embed_closed_) {
      break;
    }

    // Deal with event in main thread.
    self->WakeupMainThread();
  }
}

void OnNodePreload(node::Environment* env,
                   v8::Local<v8::Value> process,
                   v8::Local<v8::Value> require) {
  // Set custom process properties.
  gin_helper::Dictionary dict(env->isolate(), process.As<v8::Object>());
  dict.SetReadOnly("resourcesPath", GetResourcesPath());
  // base::FilePath helper_exec_path;  // path to the helper app.
  // base::PathService::Get(content::CHILD_PROCESS_EXE, &helper_exec_path);
  // dict.SetReadOnly("helperExecPath", helper_exec_path);
  gin_helper::Dictionary versions;
  if (dict.Get("versions", &versions)) {
    versions.SetReadOnly(LYNXTRON_PROJECT_NAME, LYNXTRON_VERSION_STRING);
    // versions.SetReadOnly("chrome", CHROME_VERSION_STRING);
    // TODO(Guo Xi): HAS_VENDOR_VERSION
    // #if BUILDFLAG(HAS_VENDOR_VERSION)
    //     versions.SetReadOnly(BUILDFLAG(VENDOR_VERSION_NAME),
    //                          BUILDFLAG(VENDOR_VERSION_VALUE));
    // #endif
  }

  // Execute lib/node/init.ts.
  v8::LocalVector<v8::String> bundle_params(
      env->isolate(), {node::FIXED_ONE_BYTE_STRING(env->isolate(), "process"),
                       node::FIXED_ONE_BYTE_STRING(env->isolate(), "require")});
  v8::LocalVector<v8::Value> bundle_args(env->isolate(), {process, require});
  util::CompileAndCall(env->isolate(), env->context(),
                       "lynxtron/js2c/node_init", &bundle_params, &bundle_args);
}

}  // namespace lynxtron
