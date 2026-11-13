// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef UNIPLUGIN_PLUGIN_PROCESS_H_
#define UNIPLUGIN_PLUGIN_PROCESS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "components/messaging/public/cpp/message_port_descriptor.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "uniplugin/mojom/plugin_service.mojom.h"
#include "uniplugin/native_port.h"
#include "uniplugin/public/uniplugin.h"

namespace uniplugin {
class PluginProcess : public mojo::MessageReceiver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnMessage(const std::string& message,
                           std::vector<NativePort*> ports) = 0;
    virtual void OnLaunched(bool result) = 0;
    virtual void OnDisconnected(int32_t exit_code) = 0;

   protected:
    Delegate() = default;
  };

  PluginProcess(const std::string& path, const std::string& command_line);
  ~PluginProcess() override;

  void OnDisconnected(uint32_t error_code, const std::string& description);
  void OnLaunched();

  void PostMessage(std::vector<uint8_t> message);

  bool Start();
  void OnServiceProcessDisconnected(uint32_t error_code,
                                    const std::string& description);
  void CloseConnectorPort();
  void PostMessage(const std::string& message,
                   const std::vector<NativePort*>& ports);
  void set_delegate(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

  Delegate* delegate() { return delegate_.get(); }

  ProcessHandle Handle() const { return process_.Handle(); }

  bool Terminate(int32_t exit_code, bool wait);

  // mojo::MessageReceiver
  bool Accept(mojo::Message* mojo_message) override;

 private:
  void CreateProcessRemote(mojo::ScopedMessagePipeHandle pipe);

 private:
  std::string path_;
  std::string command_line_;
  base::Process process_;
  std::unique_ptr<mojo::Connector> connector_;
  messaging::MessagePortDescriptor host_port_;
  mojo::Remote<uniplugin::mojom::PluginService> plugin_service_remote_;
  std::unique_ptr<Delegate> delegate_;
  bool connector_closed_ = false;
  base::WeakPtrFactory<PluginProcess> weak_factory_{this};
};
}  // namespace uniplugin

#endif  // UNIPLUGIN_PLUGIN_PROCESS_H_
