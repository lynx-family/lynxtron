// Copyright (c) 2022 Microsoft, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef UNIPLUGIN_PLUGIN_SERVICE_H_
#define UNIPLUGIN_PLUGIN_SERVICE_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "uniplugin/mojom/plugin_service.mojom.h"

namespace uniplugin {

class PluginService : public uniplugin::mojom::PluginService {
 public:
  explicit PluginService(
      mojo::PendingReceiver<uniplugin::mojom::PluginService> receiver);
  ~PluginService() override;

  PluginService(const PluginService&) = delete;
  PluginService& operator=(const PluginService&) = delete;

  // mojom::PluginService implementation:
  void Initialize(uniplugin::mojom::PluginServiceParamsPtr params) override;

 private:
  void LoadPlugin();

  mojo::Receiver<uniplugin::mojom::PluginService> receiver_{this};

  uniplugin::mojom::PluginServiceParamsPtr params_;

  // std::unique_ptr<PluginModule> plugin_module_;
};

}  // namespace uniplugin

#endif  // UNIPLUGIN_PLUGIN_SERVICE_H_
