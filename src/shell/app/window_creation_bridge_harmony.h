// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_APP_WINDOW_CREATION_BRIDGE_HARMONY_H_
#define LYNXTRON_SHELL_APP_WINDOW_CREATION_BRIDGE_HARMONY_H_

#include <cstdint>
#include <string>

namespace lynxtron {

struct HarmonyWindowCreationOptions;

// Callback type implemented by the OHOS NAPI bridge (lynxtron_napi_bridge.cc).
// It receives the C++-allocated window id and the creation options, then
// forwards them to the ArkTS AppWindowAdapter.
using CreateHarmonyWindowCallback =
    void (*)(int32_t window_id, const HarmonyWindowCreationOptions* options);

// Options used to create a HarmonyOS window from C++. This is the cross-layer
// contract between NativeWindowHarmony and the OHOS NAPI bridge.
struct HarmonyWindowCreationOptions {
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 800;
  int32_t height = 600;
  bool show = true;

  // Window chrome / capability flags.
  bool resizable = true;
  bool movable = true;
  bool minimizable = true;
  bool maximizable = true;
  bool closable = true;
  bool focusable = true;
  bool always_on_top = false;

  // Window type hint: "main", "sub", "float", "panel", "dialog".
  std::string type = "main";

  // Parent C++ window id. Only meaningful for sub/float/panel/dialog.
  int32_t parent_window_id = -1;

  // Parent HarmonyOS window id. Resolved by NativeWindowHarmony before calling
  // the bridge if a parent NativeWindow is provided.
  int32_t parent_harmony_window_id = -1;

  // Whether to center the window on screen when x/y are not explicitly given.
  bool center = true;
  // Whether x/y were explicitly provided in the JS options. Used by ArkTS to
  // decide whether to honor them or apply centering.
  bool has_x = false;
  bool has_y = false;

  // Window title. Forwarded to ArkTS so the initial title can be applied
  // during window creation.
  std::string title;

  // Whether the window should start in fullscreen.
  bool fullscreen = false;

  // Size constraints in pixels. 0 means no limit. Only meaningful when the
  // underlying HarmonyOS window supports size limits (e.g. free-form windows).
  int32_t min_width = 0;
  int32_t min_height = 0;
  int32_t max_width = 0;
  int32_t max_height = 0;

  // Whether the window is modal. Used for sub/panel/dialog types.
  bool modal = false;

  // Target display id. -1 means let ArkTS pick the internal display.
  int32_t display_id = -1;
};

}  // namespace lynxtron

// C-callable export consumed by lynxtron_napi_bridge.cc via dlsym.
// It forwards the creation request to the ArkTS AppWindowAdapter.
extern "C" __attribute__((visibility("default"))) void LynxtronCreateHarmonyWindow(
    int32_t window_id,
    const lynxtron::HarmonyWindowCreationOptions* options);

// Registers the callback that actually performs the NAPI call into ArkTS.
// Called once by lynxtron_napi_bridge.cc after loading liblynxtron.so.
extern "C" __attribute__((visibility("default"))) void LynxtronSetCreateHarmonyWindowCallback(
    lynxtron::CreateHarmonyWindowCallback callback);

#endif  // LYNXTRON_SHELL_APP_WINDOW_CREATION_BRIDGE_HARMONY_H_
