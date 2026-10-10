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
  // Runs at the start of MainParts::Initialize(), before the browser's V8 and
  // Node environments are set up.
  virtual void PreInitialization() = 0;
  // Runs after the Node environment is created and its main V8 context is
  // entered, before Lynxtron bindings and app code are loaded.
  virtual void PostNodeEnvironmentInitialization() = 0;
  // Runs after LoadEnvironment() and JoinAppCode(), before the embed thread
  // starts polling.
  virtual void PostInitialization() = 0;
  // Runs at the start of MainParts::Shutdown(), before the global thread and
  // thread pool are shut down.
  virtual void PreShutdown() = 0;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_MAIN_PARTS_DELEGATE_H_
