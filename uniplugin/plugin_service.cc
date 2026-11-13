// Copyright (c) 2022 Microsoft, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "uniplugin/plugin_service.h"

#include <iostream>
#include <utility>

#include "uniplugin/native_port.h"

namespace uniplugin {

namespace {
// class ServiceMessagePortDelegate : public uniplugin::NativePort::Delegate {
//  public:
//   ServiceMessagePortDelegate() = default;
//   ~ServiceMessagePortDelegate() override = default;
//   void OnMessage(const std::string& message,
//                  std::vector<uniplugin::NativePort*> ports) override {
//     LOG(INFO) << "ServiceMessagePortDelegate " << message;
//   }
//   void OnClose() override {}
// };

}  // namespace
PluginService::PluginService(
    mojo::PendingReceiver<uniplugin::mojom::PluginService> receiver) {
  LOG(INFO) << "PluginService::PluginService";
  if (receiver.is_valid()) {
    receiver_.Bind(std::move(receiver));
  }
}

PluginService::~PluginService() = default;

void PluginService::Initialize(
    uniplugin::mojom::PluginServiceParamsPtr params) {
  LOG(INFO) << "PluginService::Initialize";
  params_ = std::move(params);

  NativePort::GetParentPortInstance()->Entangle(std::move(params_->port));

  // NativePort::GetParentPortInstance()->SetDelegate(
  //     std::make_unique<ServiceMessagePortDelegate>());
  NativePort::GetParentPortInstance()->Start();
  // NativePort::GetParentPortInstance()->PostMessage("hello worldxxx", {});
  // plugin_module_ = std::make_unique<PluginModule>();
  // plugin_module_->Initialize(params_->shared_lib);
  std::cerr << "PluginService::Initialize";
}

}  // namespace uniplugin
