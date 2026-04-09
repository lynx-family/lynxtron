// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_PROCESS_TYPE_REGISTRY_H_
#define LYNXTRON_SHELL_COMMON_PROCESS_TYPE_REGISTRY_H_

#include <functional>
#include <string>
#include <unordered_map>

#include "base/command_line.h"

namespace lynxtron {

class ProcessTypeRegistry {
 public:
  using Main = std::function<int(const base::CommandLine&)>;

  explicit ProcessTypeRegistry(Main main);

  int Run(const base::CommandLine& command_line) const;

 private:
  Main main_;
};

std::unordered_map<std::string, ProcessTypeRegistry>& GetProcessTypeRegistry();

void RegisterProcessType(const std::string& name,
                         ProcessTypeRegistry::Main main);

class ProcessTypeRegistration {
 public:
  ProcessTypeRegistration(const char* name, ProcessTypeRegistry::Main main);
};

}  // namespace lynxtron

#define LYNXTRON_CONCAT_IMPL(a, b) a##b
#define LYNXTRON_CONCAT(a, b) LYNXTRON_CONCAT_IMPL(a, b)

#define REGISTER_PROCESS_TYPE(name, main)                     \
  static ::lynxtron::ProcessTypeRegistration LYNXTRON_CONCAT( \
      __process_type_registration_, __LINE__)(name, main);

#define REGISTER_PROCESS_TYPE_DELEGATE(name, main) \
  REGISTER_PROCESS_TYPE(name, main)

#endif  // LYNXTRON_SHELL_COMMON_PROCESS_TYPE_REGISTRY_H_
