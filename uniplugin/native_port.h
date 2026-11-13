// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef UNIPLUGIN_NATIVE_PORT_H_
#define UNIPLUGIN_NATIVE_PORT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/messaging/public/cpp/message_port_channel.h"
#include "components/messaging/public/cpp/message_port_descriptor.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/message.h"

namespace uniplugin {

// A non-blink version of blink::MessagePort.
class NativePort : public mojo::MessageReceiver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnClose() = 0;
    virtual void OnMessage(const std::string& message,
                           std::vector<NativePort*> ports) = 0;

   protected:
    Delegate() = default;
  };

  NativePort();
  ~NativePort() override;
  static std::unique_ptr<NativePort> Create();

  // disable copy
  NativePort(const NativePort&) = delete;
  NativePort& operator=(const NativePort&) = delete;

  void SetDelegate(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

  void PostMessage(const std::string& message,
                   const std::vector<NativePort*>& transfer);
  void Start();
  void Close();

  void Entangle(messaging::MessagePortDescriptor port);
  void Entangle(messaging::MessagePortChannel channel);

  messaging::MessagePortChannel Disentangle();

  bool IsEntangled() const { return !closed_ && !IsNeutered(); }
  bool IsNeutered() const { return !connector_ || !connector_->is_valid(); }

  static std::vector<NativePort*> EntanglePorts(
      std::vector<messaging::MessagePortChannel> channels);

  static std::vector<messaging::MessagePortChannel> DisentanglePorts(
      const std::vector<NativePort*>& ports,
      bool* threw_exception);

  static NativePort* GetParentPortInstance();

 private:
  bool HasPendingActivity() const;
  void Pin();
  void Unpin();
  void SendQueuedMessages();

  // mojo::MessageReceiver
  bool Accept(mojo::Message* mojo_message) override;

  std::unique_ptr<mojo::Connector> connector_;
  bool started_ = false;
  bool closed_ = false;
  std::vector<std::pair<std::string, std::vector<NativePort*>>>
      pending_messages_;

  std::unique_ptr<Delegate> delegate_;
  // The internal port owned by this class. The handle itself is moved into the
  // |connector_| while entangled.
  messaging::MessagePortDescriptor port_;

  base::WeakPtrFactory<NativePort> weak_factory_{this};
};
}  // namespace uniplugin

#endif  // UNIPLUGIN_NATIVE_PORT_H_
