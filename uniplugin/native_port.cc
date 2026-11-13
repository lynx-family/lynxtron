// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "uniplugin/native_port.h"

#include <string>
#include <unordered_set>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "components/messaging/public/cpp/transferable_message.h"
#include "components/messaging/public/cpp/transferable_message_mojom_traits.h"  // IWYU pragma: keep
#include "components/messaging/public/mojom/transferable_message.mojom.h"
#include "uniplugin/uniplugin_globals.h"

namespace uniplugin {

NativePort::NativePort() = default;
NativePort::~NativePort() {
  if (!IsNeutered()) {
    // Disentangle before teardown. The MessagePortDescriptor will blow up if it
    // hasn't had its underlying handle returned to it before teardown.
    Disentangle();
  }
}

void NativePort::PostMessage(const std::string& message,
                             const std::vector<NativePort*>& transfer) {
  if (!IsEntangled()) {
    // If not entangled yet, queue the message
    pending_messages_.push_back(std::make_pair(message, transfer));
    LOG(INFO) << "NativePort::PostMessage: queued message, not yet entangled";
    return;
  }
  DCHECK(!IsNeutered());

  messaging::TransferableMessage transferable_message;
  transferable_message.encoded_message =
      base::span<const uint8_t>{(const uint8_t*)message.data(), message.size()};
  transferable_message.EnsureDataIsOwned();

  // Make sure we aren't connected to any of the passed-in ports.
  for (unsigned i = 0; i < transfer.size(); ++i) {
    if (transfer[i] == this) {
      LOG(ERROR) << "Port at index " + base::NumberToString(i) +
                        " contains the source port.";
      return;
    }
  }

  bool threw_exception = false;
  transferable_message.ports =
      NativePort::DisentanglePorts(transfer, &threw_exception);
  if (threw_exception) {
    return;
  }

  mojo::Message mojo_message =
      messaging::mojom::TransferableMessage::WrapAsMessage(
          std::move(transferable_message));
  connector_->Accept(&mojo_message);
}

void NativePort::SendQueuedMessages() {
  if (!IsEntangled() || pending_messages_.empty()) {
    return;
  }

  LOG(INFO) << "NativePort::SendQueuedMessages: sending "
            << pending_messages_.size() << " queued messages";
  for (auto& msg : pending_messages_) {
    PostMessage(msg.first, msg.second);
  }
  pending_messages_.clear();
}

void NativePort::Start() {
  if (!IsEntangled()) {
    return;
  }

  if (started_) {
    return;
  }

  started_ = true;
  if (HasPendingActivity()) {
    Pin();
  }
  connector_->ResumeIncomingMethodCallProcessing();

  // Send any queued messages now that we're started and entangled
  SendQueuedMessages();
}

void NativePort::Close() {
  if (closed_) {
    return;
  }
  if (!IsNeutered()) {
    Disentangle().ReleaseHandle();
    messaging::MessagePortDescriptorPair pipe;
    Entangle(pipe.TakePort0());
  }
  closed_ = true;
  if (!HasPendingActivity()) {
    Unpin();
  }

  if (delegate_) {
    delegate_->OnClose();
  }
}

void NativePort::Entangle(messaging::MessagePortDescriptor port) {
  DCHECK(port.IsValid());
  DCHECK(!connector_);
  port_ = std::move(port);
  connector_ = std::make_unique<mojo::Connector>(
      port_.TakeHandleToEntangleWithEmbedder(),
      mojo::Connector::SINGLE_THREADED_SEND,
      uniplugin::uniplugin_task_runner());
  connector_->PauseIncomingMethodCallProcessing();
  connector_->set_incoming_receiver(this);
  connector_->set_connection_error_handler(
      base::BindOnce(&NativePort::Close, weak_factory_.GetWeakPtr()));
  if (HasPendingActivity()) {
    Pin();
  }

  // If already started, send any queued messages now that we're entangled
  if (started_) {
    SendQueuedMessages();
  }
}

void NativePort::Entangle(messaging::MessagePortChannel channel) {
  Entangle(channel.ReleaseHandle());
}

messaging::MessagePortChannel NativePort::Disentangle() {
  DCHECK(!IsNeutered());
  port_.GiveDisentangledHandle(connector_->PassMessagePipe());
  connector_ = nullptr;
  if (!HasPendingActivity()) {
    Unpin();
  }
  return messaging::MessagePortChannel(std::move(port_));
}

bool NativePort::HasPendingActivity() const {
  // The spec says that entangled message ports should always be treated as if
  // they have a strong reference.
  // We'll also stipulate that the queue needs to be open (if the app drops its
  // reference to the port before start()-ing it, then it's not really entangled
  // as it's unreachable).
  return started_ && IsEntangled();
}

// static
std::vector<NativePort*> NativePort::EntanglePorts(
    std::vector<messaging::MessagePortChannel> channels) {
  std::vector<NativePort*> wrapped_ports;
  for (auto& port : channels) {
    auto wrapped_port = new NativePort();
    wrapped_port->Entangle(std::move(port));
    wrapped_ports.emplace_back(wrapped_port);
  }
  return wrapped_ports;
}

// static
std::vector<messaging::MessagePortChannel> NativePort::DisentanglePorts(
    const std::vector<NativePort*>& ports,
    bool* threw_exception) {
  if (ports.empty()) {
    return std::vector<messaging::MessagePortChannel>();
  }

  std::unordered_set<NativePort*> visited;

  // Walk the incoming array - if there are any duplicate ports, or null ports
  // or cloned ports, throw an error (per section 8.3.3 of the HTML5 spec).
  for (unsigned i = 0; i < ports.size(); ++i) {
    auto* port = ports[i];
    if (!port || port->IsNeutered() || visited.find(port) != visited.end()) {
      std::string type;
      if (!port) {
        type = "null";
      } else if (port->IsNeutered()) {
        type = "already neutered";
      } else {
        type = "a duplicate";
      }
      LOG(ERROR) << "Port at index " + base::NumberToString(i) + " is " + type +
                        ".";
      *threw_exception = true;
      return std::vector<messaging::MessagePortChannel>();
    }
    visited.insert(port);
  }

  // Passed-in ports passed validity checks, so we can disentangle them.
  std::vector<messaging::MessagePortChannel> channels;
  channels.reserve(ports.size());
  for (const auto& port : ports) {
    channels.push_back(port->Disentangle());
  }
  return channels;
}

// static
NativePort* NativePort::GetParentPortInstance() {
  static base::NoDestructor<NativePort> instance;
  return instance.get();
}

void NativePort::Pin() {}

void NativePort::Unpin() {}

bool NativePort::Accept(mojo::Message* mojo_message) {
  LOG(INFO) << "NativePort::Accept";
  messaging::TransferableMessage message;
  if (!messaging::mojom::TransferableMessage::DeserializeFromMessage(
          std::move(*mojo_message), &message)) {
    LOG(ERROR) << "NativePort::Accept DeserializeFromMessage error";
    return false;
  }

  auto ports = EntanglePorts(std::move(message.ports));

  std::string message_value{
      reinterpret_cast<const char*>(message.encoded_message.data()),
      message.encoded_message.size()};
  if (delegate_) {
    delegate_->OnMessage(message_value, std::move(ports));
  }
  return true;
}
}  // namespace uniplugin
