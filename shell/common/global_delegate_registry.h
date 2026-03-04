// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SHELL_COMMON_GLOBAL_DELEGATE_REGISTRY_H_
#define SHELL_COMMON_GLOBAL_DELEGATE_REGISTRY_H_

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace lynxtron {
class GlobalDelegate {
 public:
  virtual ~GlobalDelegate() = default;
};

class GlobalDelegateRegistry {
 public:
  using Constructor = std::function<std::unique_ptr<GlobalDelegate>()>;
  explicit GlobalDelegateRegistry(Constructor ctor);

  std::unique_ptr<GlobalDelegate> CreateDelegate() const;

 private:
  Constructor constructor;
};

std::unordered_map<std::string, GlobalDelegateRegistry>&
GetGlobalDelegateRegistry();

void RegisterGlobalDelegate(const std::string& name,
                            GlobalDelegateRegistry::Constructor ctor);

}  // namespace lynxtron

#define REGISTER_GLOBAL_DELEGATE(name, class_type)              \
  static void __register_global_delegate_##class_type(void)     \
      __attribute__((constructor));                             \
  static void __register_global_delegate_##class_type(void) {   \
    lynxtron::RegisterGlobalDelegate(                           \
        name, []() { return std::make_unique<class_type>(); }); \
  }

#endif  // SHELL_COMMON_GLOBAL_DELEGATE_REGISTRY_H
