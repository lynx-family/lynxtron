// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/process_type_registry.h"

#include <utility>

namespace lynxtron {

ProcessTypeRegistry::ProcessTypeRegistry(Main main) : main_(std::move(main)) {}

int ProcessTypeRegistry::Run(const base::CommandLine& command_line) const {
  if (main_) {
    return main_(command_line);
  }
  return -1;
}

std::unordered_map<std::string, ProcessTypeRegistry>& GetProcessTypeRegistry() {
  static std::unordered_map<std::string, ProcessTypeRegistry> registry;
  return registry;
}

void RegisterProcessType(const std::string& name,
                         ProcessTypeRegistry::Main main) {
  GetProcessTypeRegistry().emplace(name, ProcessTypeRegistry(std::move(main)));
}

ProcessTypeRegistration::ProcessTypeRegistration(
    const char* name,
    ProcessTypeRegistry::Main main) {
  RegisterProcessType(name, std::move(main));
}

}  // namespace lynxtron
