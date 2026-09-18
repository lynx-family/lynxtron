// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/window_creation_bridge_harmony.h"

#include <hilog/log.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0000
#define LOG_TAG "LynxtronWindow"

namespace lynxtron {

namespace {

CreateHarmonyWindowCallback g_create_window_callback = nullptr;

}  // namespace

}  // namespace lynxtron

// Stores the NAPI bridge callback that forwards C++ window creation requests
// to the ArkTS AppWindowAdapter.
extern "C" __attribute__((visibility("default"))) void LynxtronSetCreateHarmonyWindowCallback(
    lynxtron::CreateHarmonyWindowCallback callback) {
  lynxtron::g_create_window_callback = callback;
  OH_LOG_INFO(LOG_APP, "[LynxtronWindow] create-window callback registered=%{public}d",
              callback != nullptr);
}

// Validates a window creation request and forwards it to the registered
// ArkTS create-window callback.
extern "C" __attribute__((visibility("default"))) void LynxtronCreateHarmonyWindow(
    int32_t window_id,
    const lynxtron::HarmonyWindowCreationOptions* options) {
  if (!options) {
    OH_LOG_ERROR(LOG_APP, "[LynxtronWindow] LynxtronCreateHarmonyWindow called with null options");
    return;
  }
  if (!lynxtron::g_create_window_callback) {
    OH_LOG_ERROR(LOG_APP,
                 "[LynxtronWindow] no create-window callback registered for id=%{public}d",
                 window_id);
    return;
  }
  OH_LOG_INFO(LOG_APP,
              "[LynxtronWindow] requesting creation id=%{public}d type=%{public}s "
              "bounds=%{public}d,%{public}d,%{public}dx%{public}d display_id=%{public}d",
              window_id, options->type.c_str(), options->x, options->y,
              options->width, options->height, options->display_id);
  lynxtron::g_create_window_callback(window_id, options);
}
