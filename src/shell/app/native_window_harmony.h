// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SHELL_APP_NATIVE_WINDOW_HARMONY_H_
#define SHELL_APP_NATIVE_WINDOW_HARMONY_H_

#include <cstdint>

namespace lynxtron {

// Called by the XComponent surface lifecycle when ArkUI changes the content
// size. The update is delivered on Lynxtron's UI sequence so its LynxView
// updates the viewport used for both rendering and pointer hit testing.
void UpdateHarmonyNativeWindowSize(int width, int height);

// Per-window variant. The NAPI bridge sets a thread-local current window id
// from setWindowId/setWindowIdForWindow and passes it through the surface
// callback so multi-window setups route size updates correctly.
void UpdateHarmonyNativeWindowSizeForWindow(int32_t harmony_window_id,
                                            int width,
                                            int height);

// Associates the XComponent native surface with the matching C++ window.
// LynxViewBuilder uses this handle to select the renderer for that window.
void UpdateHarmonyNativeWindowSurfaceForWindow(int32_t harmony_window_id,
                                               void* surface);

// Exported from native_window_harmony.cc with C linkage. Declared inside the
// namespace so surface_render_harmony.cc can call it as lynxtron::....
extern "C" void LynxtronSetHarmonySurfaceSize(int width, int height);
extern "C" void LynxtronSetHarmonySurfaceSizeForWindow(int32_t harmony_window_id,
                                                       int width,
                                                       int height);

// Called from lynxtron_napi_bridge.cc when the ArkTS side reports the real
// window rect (windowRect), which includes decorations/avoidance areas.
extern "C" void LynxtronNotifyWindowRect(int32_t harmony_window_id,
                                         int32_t x,
                                         int32_t y,
                                         int32_t width,
                                         int32_t height);

}  // namespace lynxtron

#endif  // SHELL_APP_NATIVE_WINDOW_HARMONY_H_
