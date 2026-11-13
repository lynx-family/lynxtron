// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "uniplugin/public/uniplugin.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "uniplugin/mojom/plugin_service.mojom.h"
#include "uniplugin/native_port.h"
#include "uniplugin/plugin_process.h"
#include "uniplugin/plugin_service.h"
#include "uniplugin/process_callbacks.h"
#include "uniplugin/uniplugin_globals.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if BUILDFLAG(IS_MAC)
#include <CoreFoundation/CoreFoundation.h>

#include "base/apple/foundation_util.h"
#include "base/apple/mach_port_rendezvous.h"
#include "base/strings/string_util.h"

namespace {

void SetUpBundleOverrides() {
  base::FilePath exe_path;
  base::PathService::Get(base::FILE_EXE, &exe_path);
  LOG(INFO) << "[uniplugin] SetUpBundleOverrides exe_path=" << exe_path.value();

  base::FilePath contents_path;
  std::string exe_name = exe_path.BaseName().value();
  if (base::EndsWith(exe_name, " Helper", base::CompareCase::SENSITIVE)) {
    contents_path = exe_path.DirName().DirName().DirName().DirName().DirName();
    LOG(INFO) << "[uniplugin] Helper process detected";
  } else {
    contents_path = exe_path.DirName().DirName();
  }
  base::FilePath app_path = contents_path.DirName();
  LOG(INFO) << "[uniplugin] app_path=" << app_path.value();

  CFURLRef app_url = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault,
      reinterpret_cast<const UInt8*>(app_path.value().c_str()),
      app_path.value().length(), true);
  if (!app_url) {
    LOG(ERROR) << "[uniplugin] Failed to create app_url";
    return;
  }

  CFBundleRef app_bundle = CFBundleCreate(kCFAllocatorDefault, app_url);
  CFRelease(app_url);
  if (!app_bundle) {
    LOG(ERROR) << "[uniplugin] Failed to create app_bundle";
    return;
  }

  CFStringRef bundle_id = CFBundleGetIdentifier(app_bundle);
  if (bundle_id) {
    char id_buf[256];
    if (CFStringGetCString(bundle_id, id_buf, sizeof(id_buf),
                           kCFStringEncodingUTF8)) {
      std::string base_bundle_id(id_buf);
      LOG(INFO) << "[uniplugin] bundle_id=" << base_bundle_id;
      CFStringRef team_id =
          static_cast<CFStringRef>(CFBundleGetValueForInfoDictionaryKey(
              app_bundle, CFSTR("ElectronTeamID")));
      if (team_id && CFGetTypeID(team_id) == CFStringGetTypeID()) {
        char team_buf[64];
        if (CFStringGetCString(team_id, team_buf, sizeof(team_buf),
                               kCFStringEncodingUTF8)) {
          base_bundle_id = std::string(team_buf) + "." + base_bundle_id;
        }
      } else {
        LOG(WARNING) << "[uniplugin] ElectronTeamID not found in bundle";
      }
      LOG(INFO) << "[uniplugin] SetBaseBundleID=" << base_bundle_id;
      base::apple::SetBaseBundleIDOverride(base_bundle_id);
    }
  } else {
    LOG(ERROR) << "[uniplugin] bundle has no identifier";
  }
  CFRelease(app_bundle);
}

}  // namespace
#endif

[[maybe_unused]] const char kProcessType[] = "type";

void InitLogging(const base::FilePath& log_file_path) {
  // 创建 LoggingSettings 对象
  logging::LoggingSettings log_settings;
  // 设置日志文件路径
  log_settings.log_file_path = log_file_path.value().c_str();
  // 设置日志记录选项，例如是否记录到文件、输出到控制台等
  log_settings.logging_dest = logging::LOG_TO_ALL;
  // 初始化日志记录系统
  logging::InitLogging(log_settings);
  // 设置日志级别（可选）
  logging::SetMinLogLevel(logging::LOGGING_VERBOSE);
  // 设置日志格式（可选）
  logging::SetLogItems(true,   // 设置是否显示进程ID
                       true,   // 设置是否显示线程ID
                       true,   // 设置是否显示时间戳
                       true);  // 设置是否显示日志级别
}

void UPChildProcessStart(int argc, char* argv[]) {
  uniplugin::uniplugin_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](int argc, char* argv[]) {
            base::CommandLine::Init(argc, argv);

            const base::CommandLine& cmd =
                *base::CommandLine::ForCurrentProcess();
            LOG(INFO) << "[uniplugin] child cmdline="
                      << cmd.GetCommandLineString();
#if BUILDFLAG(IS_MAC)
            LOG(INFO) << "[uniplugin] BaseBundleID="
                      << base::apple::BaseBundleID();
#endif

            std::string handle_value =
                cmd.GetSwitchValueASCII("mojo-platform-channel-handle");
            LOG(INFO) << "[uniplugin] handle_switch=" << handle_value;

            auto endpoint =
                mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
                    cmd);
            LOG(INFO) << "[uniplugin] endpoint.is_valid="
                      << endpoint.is_valid();

            if (!endpoint.is_valid()) {
              LOG(ERROR) << "[uniplugin] endpoint is INVALID, cannot accept "
                            "invitation";
              return;
            }

            mojo::IncomingInvitation invitation =
                mojo::IncomingInvitation::Accept(std::move(endpoint));
            mojo::ScopedMessagePipeHandle pipe =
                invitation.ExtractMessagePipe("pipe");

            mojo::PendingReceiver<uniplugin::mojom::PluginService>
                pending_receiver(std::move(pipe));
            uniplugin::UnipluginGlobal::GetInstance().set_process_service(
                std::make_unique<uniplugin::PluginService>(
                    std::move(pending_receiver)));
          },
          argc, argv));
}

UNIPLUGIN_EXPORT void UPInitialize(bool is_broker_process, bool enable_log) {
  base::FilePath executable_path;
  base::PathService::Get(base::FILE_EXE, &executable_path);
  if (enable_log) {
    InitLogging(executable_path.Append(FILE_PATH_LITERAL("uniplugin.log")));
  }

  LOG(INFO) << " ================ UnipluginStart ================= ";

#if BUILDFLAG(IS_MAC)
  SetUpBundleOverrides();
#endif

  mojo::core::Configuration config;
  config.is_broker_process = is_broker_process;
  mojo::core::Init(config);
  uniplugin::UnipluginGlobal::GetInstance().Start();
}

void UPUninitialize() {
  uniplugin::UnipluginGlobal::GetInstance().Stop();
}

bool UPCreateMessageChannel(UPMessagePort* port1, UPMessagePort* port2) {
  if (port1 == nullptr || port2 == nullptr) {
    return false;
  }

  auto* native_port1 = new uniplugin::NativePort();
  auto* native_port2 = new uniplugin::NativePort();
  *port1 = reinterpret_cast<UPMessagePort>(native_port1);
  *port2 = reinterpret_cast<UPMessagePort>(native_port2);

  uniplugin::uniplugin_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](uniplugin::NativePort* native_port1,
                        uniplugin::NativePort* native_port2) {
                       messaging::MessagePortDescriptorPair pipe;
                       native_port1->Entangle(pipe.TakePort0());
                       native_port2->Entangle(pipe.TakePort1());
                     },
                     native_port1, native_port2));
  return true;
}

void UPMessagePortDestroy(UPMessagePort port) {
  if (port == nullptr) {
    return;
  }
  auto* native_port = reinterpret_cast<uniplugin::NativePort*>(port);
  auto task_runner = uniplugin::uniplugin_task_runner();
  if (task_runner->RunsTasksInCurrentSequence()) {
    delete native_port;
    return;
  }

  base::WaitableEvent destroyed;
  bool posted = task_runner->PostTask(FROM_HERE,
                                      base::BindOnce(
                                          [](uniplugin::NativePort* native_port,
                                             base::WaitableEvent* destroyed) {
                                            delete native_port;
                                            destroyed->Signal();
                                          },
                                          native_port, &destroyed));
  if (!posted) {
    LOG(ERROR) << "Failed to schedule MessagePort destruction";
    return;
  }
  destroyed.Wait();
}

void UPMessagePortStart(UPMessagePort port) {
  if (port == nullptr) {
    return;
  }
  auto* native_port = reinterpret_cast<uniplugin::NativePort*>(port);
  native_port->Start();
}

void UPMessagePortPostMessage(UPMessagePort port,
                              const char* message,
                              uint32_t length,
                              UPMessagePort* ports,
                              uint32_t port_count) {
  auto* native_port = reinterpret_cast<uniplugin::NativePort*>(port);
  if (native_port == nullptr) {
    return;
  }

  std::vector<uniplugin::NativePort*> native_ports;
  for (size_t i = 0; i < port_count; i++) {
    auto* native_port_obj = reinterpret_cast<uniplugin::NativePort*>(ports[i]);
    native_ports.push_back(native_port_obj);
  }

  uniplugin::uniplugin_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](uniplugin::NativePort* native_port, std::string message,
             std::vector<uniplugin::NativePort*> native_ports) {
            native_port->PostMessage(message, native_ports);
          },
          native_port, std::string(message, length), std::move(native_ports)));
}

class MessagePortDelegate : public uniplugin::NativePort::Delegate {
 public:
  explicit MessagePortDelegate(UPMessagePortMessageCallback callback,
                               UPConnectionCloseCallback close_callback)
      : callback_(callback), close_callback_(close_callback) {}
  ~MessagePortDelegate() override = default;
  void OnMessage(const std::string& message,
                 std::vector<uniplugin::NativePort*> ports) override {
    if (callback_ != nullptr) {
      callback_(message.c_str(), message.length(),
                reinterpret_cast<UPMessagePort*>(ports.data()), ports.size());
      LOG(INFO) << " MessagePortDelegate OnMessage " << message.c_str();
    }
  }
  void OnClose() override {
    LOG(INFO) << " MessagePortDelegate OnClose ";
    if (close_callback_ != nullptr) {
      close_callback_(0);
    }
  }

 private:
  UPMessagePortMessageCallback callback_;
  UPConnectionCloseCallback close_callback_;
};

void UPMessagePortSetMessageCallback(UPMessagePort port,
                                     UPMessagePortMessageCallback callback) {
  auto* native_port = reinterpret_cast<uniplugin::NativePort*>(port);
  if (native_port == nullptr) {
    return;
  }
  native_port->SetDelegate(
      std::make_unique<MessagePortDelegate>(callback, nullptr));
}

// void UPMessagePortSetMessageErrorCallback(
//     UPMessagePort port,
//     UPMessagePortMessageCallback callback) {}

UPPluginProcess UPPluginProcessCreate(UPCommandLineArgs* args) {
  if (args == nullptr) {
    return nullptr;
  }
  auto* plugin_process =
      new uniplugin::PluginProcess(args->path, args->command_lines);
  return reinterpret_cast<UPPluginProcess>(plugin_process);
}

bool UPPluginProcessFork(UPPluginProcess process) {
  if (process == nullptr) {
    return false;
  }

  auto* plugin_process = reinterpret_cast<uniplugin::PluginProcess*>(process);
  uniplugin::uniplugin_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](uniplugin::PluginProcess* process) { process->Start(); },
          plugin_process));
  return true;
}

void UPPluginProcessDestroy(UPPluginProcess process) {
  auto* plugin_process = reinterpret_cast<uniplugin::PluginProcess*>(process);
  if (plugin_process == nullptr) {
    return;
  }

  uniplugin::uniplugin_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](uniplugin::PluginProcess* process) {
                       process->Terminate(0, true);
                       delete process;
                     },
                     plugin_process));
}

// 销毁进程对象
UNIPLUGIN_EXPORT void UPPluginProcessCloseConnectorPort(
    UPPluginProcess process) {
  auto* plugin_process = reinterpret_cast<uniplugin::PluginProcess*>(process);
  if (plugin_process == nullptr) {
    return;
  }

  uniplugin::uniplugin_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](uniplugin::PluginProcess* process) {
                       process->CloseConnectorPort();
                     },
                     plugin_process));
}
UNIPLUGIN_EXPORT void UPPluginProcessPostMessage(UPPluginProcess process,
                                                 const char* message,
                                                 uint32_t length,
                                                 UPMessagePort* ports,
                                                 uint32_t port_count) {
  uniplugin::PluginProcess* plugin_process =
      reinterpret_cast<uniplugin::PluginProcess*>(process);
  if (plugin_process == nullptr) {
    return;
  }
  std::vector<uniplugin::NativePort*> native_ports;
  for (size_t i = 0; i < port_count; i++) {
    auto* native_port = reinterpret_cast<uniplugin::NativePort*>(ports[i]);
    native_ports.push_back(native_port);
  }

  uniplugin::uniplugin_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](uniplugin::PluginProcess* plugin_process, std::string message,
             std::vector<uniplugin::NativePort*> native_ports) {
            plugin_process->PostMessage(message, native_ports);
          },
          plugin_process, std::string(message, length),
          std::move(native_ports)));
}

class PluginProcessMessageDelegate : public uniplugin::PluginProcess::Delegate {
 public:
  explicit PluginProcessMessageDelegate(uniplugin::ProcessCallbacks* callbacks,
                                        UPPluginProcess process)
      : callbacks_(callbacks), process_(process) {}
  ~PluginProcessMessageDelegate() override = default;
  void OnMessage(const std::string& message,
                 std::vector<uniplugin::NativePort*> ports) override {
    if (callbacks_ && callbacks_->message_callback()) {
      callbacks_->message_callback()(
          message.c_str(), message.length(),
          reinterpret_cast<UPMessagePort*>(ports.data()), ports.size());
      LOG(INFO) << " PluginProcessMessageDelegate OnMessage "
                << message.c_str();
    }
  }

  void OnLaunched(bool result) override {
    if (callbacks_ && callbacks_->spawn_callback()) {
      callbacks_->spawn_callback()();
    }
  }

  void OnDisconnected(int exit_code) override {
    if (callbacks_ && callbacks_->exit_callback()) {
      callbacks_->exit_callback()(process_, exit_code);
    }
  }

 private:
  // UPMessagePortMessageCallback callback_;
  uniplugin::ProcessCallbacks* callbacks_;
  UPPluginProcess process_;
  // UPMess
};

void UPPluginProcessSetMessageCallback(
    UPPluginProcess process,
    UPMessagePortMessageCallback callback,
    UPChildProcessCloseCallback exit_callback) {
  auto* plugin_process = reinterpret_cast<uniplugin::PluginProcess*>(process);
  if (plugin_process == nullptr) {
    return;
  }
  if (auto* process_callbacks =
          uniplugin::UnipluginGlobal::GetInstance().process_callbacks()) {
    process_callbacks->set_message_callback(callback);
    process_callbacks->set_exit_callback(exit_callback);
    if (plugin_process->delegate() == nullptr) {
      plugin_process->set_delegate(
          std::make_unique<PluginProcessMessageDelegate>(process_callbacks,
                                                         process));
    }
  }

  // plugin_process->set_delegate(
  //     std::make_unique<PluginProcessMessageDelegate>(callback));
}

ProcessHandle UPPluginProcessGetNativeHandle(UPPluginProcess process) {
  auto* plugin_process = reinterpret_cast<uniplugin::PluginProcess*>(process);
  if (plugin_process == nullptr) {
    return kNullProcessHandle;
  }
  return plugin_process->Handle();
}

// spawn 回调
void UPPluginProcessSetSpawnCallback(UPPluginProcess obj,
                                     UPPluginProcessSpawnCallback callback) {}

// exit 回调
void UPPluginProcessSetExitCallback(UPPluginProcess obj,
                                    UPPluginProcessExitCallback callback) {}

// bool UPPluginProcessChildInit(int argc, char* argv[]) {
//   LOG(INFO) << "================ ProcessChild ===========";
//   uniplugin::uniplugin_task_runner()->PostTask(
//       FROM_HERE,
//       base::BindOnce(
//           [](int argc, char* argv[]) {
//             base::CommandLine::Init(argc, argv);

//             // Accept an invitation.
//             mojo::IncomingInvitation invitation =
//                 mojo::IncomingInvitation::Accept(
//                     mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
//                         *base::CommandLine::ForCurrentProcess()));
//             mojo::ScopedMessagePipeHandle pipe =
//                 invitation.ExtractMessagePipe("pipe");
//           },
//           argc, argv));

//   return false;
// }
void UPChildProcessSetMessageCallback(
    UPMessagePortMessageCallback callback,
    UPConnectionCloseCallback close_callback) {
  uniplugin::NativePort::GetParentPortInstance()->SetDelegate(
      std::make_unique<MessagePortDelegate>(callback, close_callback));
}

UNIPLUGIN_EXPORT void UPChildProcessPostMessage(const char* message,
                                                uint32_t length,
                                                UPMessagePort* ports,
                                                uint32_t port_count) {
  std::vector<uniplugin::NativePort*> native_ports;
  for (size_t i = 0; i < port_count; i++) {
    auto* native_port = reinterpret_cast<uniplugin::NativePort*>(ports[i]);
    native_ports.push_back(native_port);
  }
  uniplugin::uniplugin_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::string message,
             std::vector<uniplugin::NativePort*> native_ports) {
            LOG(INFO) << " UPChildProcessPostMessage " << message.c_str();
            uniplugin::NativePort::GetParentPortInstance()->PostMessage(
                message, native_ports);
          },
          std::string(message, length), std::move(native_ports)));
}
