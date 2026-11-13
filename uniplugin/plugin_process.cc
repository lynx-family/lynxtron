// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "uniplugin/plugin_process.h"

#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "components/messaging/public/cpp/message_port_descriptor.h"
#include "components/messaging/public/cpp/transferable_message.h"
#include "components/messaging/public/cpp/transferable_message_mojom_traits.h"  // IWYU pragma: keep
#include "components/messaging/public/mojom/transferable_message.mojom.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "uniplugin/native_port.h"
#include "uniplugin/uniplugin_globals.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

[[maybe_unused]] const char kProcessType[] = "type";

namespace uniplugin {

PluginProcess::PluginProcess(const std::string& path,
                             const std::string& command_line)
    : path_(path), command_line_(command_line) {}
PluginProcess::~PluginProcess() = default;

void PluginProcess::CreateProcessRemote(
    mojo::ScopedMessagePipeHandle child_pipe) {
  mojo::PendingRemote<uniplugin::mojom::PluginService> pending_remote(
      std::move(child_pipe), 0u);
  // mojo::Remote<uniplugin::mojom::PluginService> remote(
  //     std::move(pending_remote));

  //  mojo::PendingReceiver<uniplugin::mojom::PluginService> receiver =
  //      plugin_service_remote_.BindNewPipeAndPassReceiver();

  // std::ignore = plugin_service_remote_.BindNewPipeAndPassReceiver();
  plugin_service_remote_.Bind(std::move(pending_remote));

  plugin_service_remote_.set_disconnect_with_reason_handler(
      base::BindOnce(&PluginProcess::OnServiceProcessDisconnected,
                     weak_factory_.GetWeakPtr()));

  // We use a separate message pipe to support postMessage API
  // instead of the existing receiver interface so that we can
  // support queuing of messages without having to block other
  // interfaces.
  messaging::MessagePortDescriptorPair pipe;
  host_port_ = pipe.TakePort0();

  uniplugin::mojom::PluginServiceParamsPtr params =
      uniplugin::mojom::PluginServiceParams::New();
  params->port = pipe.TakePort1();
  LOG(INFO) << "PluginProcess::Connector created this " << this;
  connector_ = std::make_unique<mojo::Connector>(
      host_port_.TakeHandleToEntangleWithEmbedder(),
      mojo::Connector::SINGLE_THREADED_SEND,
      uniplugin::uniplugin_task_runner());
  connector_->set_incoming_receiver(this);
  connector_->set_connection_error_handler(base::BindOnce(
      &PluginProcess::CloseConnectorPort, weak_factory_.GetWeakPtr()));

  plugin_service_remote_->Initialize(std::move(params));
}

bool PluginProcess::Start() {
  mojo::PlatformChannel channel;
  mojo::OutgoingInvitation invitation;
  mojo::ScopedMessagePipeHandle child_pipe =
      invitation.AttachMessagePipe("pipe");

  base::FilePath executable_path(base::FilePath::FromUTF8Unsafe(path_.c_str()));
  base::LaunchOptions options;
#if BUILDFLAG(IS_WIN)

  // std::wstring command_line_widge;
  // base::UTF8ToWide(command_line_.c_str(), command_line_.length(),
  //                  &command_line_widge);
  base::CommandLine cmd(executable_path);
  std::string command_line = command_line_;
  std::vector<std::string> args;

  // Trim leading/trailing whitespace
  size_t start = command_line.find_first_not_of(" \t");
  size_t end = command_line.find_last_not_of(" \t");
  if (start != std::string::npos && end != std::string::npos) {
    command_line = command_line.substr(start, end - start + 1);
  }
  // Parse arguments (simple implementation, handles basic cases)
  std::istringstream iss(command_line);
  std::string arg;
  bool in_quotes = false;
  std::string current_arg;

  for (char c : command_line) {
    if (c == '"' && !in_quotes) {
      in_quotes = true;
    } else if (c == '"' && in_quotes) {
      in_quotes = false;
    } else if (c == ' ' && !in_quotes) {
      if (!current_arg.empty()) {
        args.push_back(current_arg);
        current_arg.clear();
      }
    } else {
      current_arg += c;
    }
  }
  // Add the last argument if any
  if (!current_arg.empty()) {
    args.push_back(current_arg);
  }
  // Add all parsed arguments to command line
  for (const auto& argument : args) {
    cmd.AppendArg(argument);
  }
  // cmd.SetProgram(executable_path);
#else
  // TODO(Guo Xi): add macOS implementation
  base::CommandLine cmd(executable_path);
  std::string command_line = command_line_;
  std::vector<std::string> args;

  // Trim leading/trailing whitespace
  size_t start = command_line.find_first_not_of(" \t");
  size_t end = command_line.find_last_not_of(" \t");
  if (start != std::string::npos && end != std::string::npos) {
    command_line = command_line.substr(start, end - start + 1);
  }
  // Parse arguments (simple implementation, handles basic cases)
  std::istringstream iss(command_line);
  std::string arg;
  bool in_quotes = false;
  std::string current_arg;

  for (char c : command_line) {
    if (c == '"' && !in_quotes) {
      in_quotes = true;
    } else if (c == '"' && in_quotes) {
      in_quotes = false;
    } else if (c == ' ' && !in_quotes) {
      if (!current_arg.empty()) {
        args.push_back(current_arg);
        current_arg.clear();
      }
    } else {
      current_arg += c;
    }
  }
  // Add the last argument if any
  if (!current_arg.empty()) {
    args.push_back(current_arg);
  }
  // Add all parsed arguments to command line
  for (const auto& argument : args) {
    cmd.AppendArg(argument);
  }
#endif

  // cmd.AppendSwitchASCII(kProcessType, "child");

  // Set up the command line to pass to the child process.
  channel.PrepareToPassRemoteEndpoint(&options, &cmd);
  process_ = base::LaunchProcess(cmd, options);
  if (!process_.IsValid()) {
    if (delegate_) {
      delegate_->OnLaunched(false);
    }
    return false;
  }

  LOG(INFO) << "Create child process successfully";

  channel.RemoteProcessLaunchAttempted();

  mojo::OutgoingInvitation::Send(std::move(invitation), process_.Handle(),
                                 channel.TakeLocalEndpoint());

  CreateProcessRemote(std::move(child_pipe));
  if (delegate_) {
    delegate_->OnLaunched(true);
  }
  LOG(INFO) << "end of process start";
  return true;
}

bool PluginProcess::Accept(mojo::Message* mojo_message) {
  messaging::TransferableMessage message;
  if (!messaging::mojom::TransferableMessage::DeserializeFromMessage(
          std::move(*mojo_message), &message)) {
    return false;
  }
  auto ports = NativePort::EntanglePorts(std::move(message.ports));

  std::string message_value{
      reinterpret_cast<const char*>(message.encoded_message.data()),
      message.encoded_message.size()};
  if (delegate_) {
    delegate_->OnMessage(message_value, std::move(ports));
  }

  // v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  // v8::HandleScope handle_scope(isolate);
  // // save into v8 arraybuffer
  // v8::Local<v8::ArrayBuffer> array_buffer =
  //     v8::ArrayBuffer::New(isolate, message.encoded_message.size());
  // std::memcpy(array_buffer->GetBackingStore()->Data(),
  //             message.encoded_message.data(),
  //             message.encoded_message.size());
  // v8::Local<v8::Uint8Array> array =
  //     v8::Uint8Array::New(array_buffer, 0, message.encoded_message.size());
  // v8::Local<v8::Value> message_value = array.As<v8::Value>();

  // EmitWithoutCustomEvent("message", message_value);
  return true;
}

void PluginProcess::OnServiceProcessDisconnected(
    uint32_t error_code,
    const std::string& description) {
  LOG(ERROR) << "PluginProcess::OnServiceProcessDisconnected " << description
             << " error_code " << error_code;
  if (!delegate_) {
    return;
  }
  int32_t exit_code = 0;
  if (process_.IsValid()) {
#if BUILDFLAG(IS_WIN)
    auto status = base::GetTerminationStatus(process_.Handle(), &exit_code);
#else
    auto status =
        base::GetKnownDeadTerminationStatus(process_.Handle(), &exit_code);
#endif
    LOG(INFO) << "PluginProcess TerminationStatus "
              << " exit_code " << exit_code << " status " << status;
    switch (status) {
      case base::TERMINATION_STATUS_PROCESS_CRASHED:
      case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
        exit_code = -1;
        break;
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
        exit_code = 1;
        break;
      default:
        exit_code = 0;
        break;
    }
  }
  delegate_->OnDisconnected(exit_code);
}

void PluginProcess::PostMessage(const std::string& message,
                                const std::vector<NativePort*>& ports) {
  LOG(INFO) << "PluginProcess::PostMessage " << message;
  messaging::TransferableMessage transferable_message;
  transferable_message.encoded_message =
      base::span<const uint8_t>{(const uint8_t*)message.data(), message.size()};
  transferable_message.EnsureDataIsOwned();

  bool threw_exception = false;
  transferable_message.ports =
      NativePort::DisentanglePorts(ports, &threw_exception);
  if (threw_exception) {
    return;
  }

  mojo::Message mojo_message =
      messaging::mojom::TransferableMessage::WrapAsMessage(
          std::move(transferable_message));
  LOG(INFO) << "PluginProcess::PostMessage Accept" << message << " this "
            << this;
  if (!connector_ || !connector_->is_valid()) {
    LOG(ERROR) << "PluginProcess::PostMessage connector_ is null";
    return;
  }
  connector_->Accept(&mojo_message);
}

void PluginProcess::CloseConnectorPort() {
  LOG(INFO) << "PluginProcess::CloseConnectorPort";
  if (!connector_closed_ && connector_->is_valid()) {
    host_port_.GiveDisentangledHandle(connector_->PassMessagePipe());
    connector_ = nullptr;
    host_port_.Reset();
    connector_closed_ = true;
  }
}

bool PluginProcess::Terminate(int32_t exit_code, bool wait) {
  if (!process_.IsValid()) {
    return false;
  }
  bool result = process_.Terminate(exit_code, wait);
  process_.Close();
  return result;
}

}  // namespace uniplugin
