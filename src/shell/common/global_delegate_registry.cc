// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/global_delegate_registry.h"

#include <utility>

namespace lynxtron {

GlobalDelegateRegistry::GlobalDelegateRegistry(Constructor ctor)
    : constructor(std::move(ctor)) {}

std::unique_ptr<GlobalDelegate> GlobalDelegateRegistry::CreateDelegate() const {
  if (constructor) {
    return constructor();
  }
  return nullptr;
}

std::unordered_map<std::string, GlobalDelegateRegistry>&
GetGlobalDelegateRegistry() {
  static std::unordered_map<std::string, GlobalDelegateRegistry> registry;
  return registry;
}

void RegisterGlobalDelegate(const std::string& name,
                            GlobalDelegateRegistry::Constructor ctor) {
  GetGlobalDelegateRegistry().emplace(name,
                                      GlobalDelegateRegistry(std::move(ctor)));
}

}  // namespace lynxtron
