// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_window_manager.h"

#include "base/no_destructor.h"

namespace lynxtron {
namespace api {

LynxWindowManager* LynxWindowManager::GetInstance() {
  static ::base::NoDestructor<LynxWindowManager> manager;
  return manager.get();
}

LynxWindowManager::LynxWindowManager() = default;

LynxWindowManager::~LynxWindowManager() = default;

void LynxWindowManager::RegisterLynxWindow(
    base::WeakPtr<LynxWindow> lynx_window) {
  lynx_window_set_.insert(lynx_window);
}

void LynxWindowManager::UnregisterLynxWindow(
    base::WeakPtr<LynxWindow> lynx_window) {
  lynx_window_set_.erase(lynx_window);
}

}  // namespace api
}  // namespace lynxtron
