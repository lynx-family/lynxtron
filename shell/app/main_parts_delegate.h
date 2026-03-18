// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_APP_MAIN_PARTS_DELEGATE_H_
#define LYNXTRON_SHELL_APP_MAIN_PARTS_DELEGATE_H_

#include "shell/common/global_delegate_registry.h"

namespace lynxtron {

static const char* kMainPartsDelegateName = "MainPartsDelegate";

class MainPartsDelegate : public GlobalDelegate {
 public:
  virtual void PreInitialization() = 0;
  virtual void PostV8Initialization() = 0;
  virtual void PostInitialization() = 0;
  virtual void PreShutdown() = 0;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_MAIN_PARTS_DELEGATE_H_
