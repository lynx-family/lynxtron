// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SHELL_APP_LYNX_BRINGUP_HARMONY_H_
#define SHELL_APP_LYNX_BRINGUP_HARMONY_H_

#include <cstdint>

namespace lynxtron {
class LynxView;

// Roadmap step 2 bring-up shortcut: once the XComponent surface exists, build a
// LynxView (which attaches the GLDirect windowless renderer from step 1) and
// load a staged test bundle (resfile/resources/main.lynx.bundle) directly from
// C++, so we get a first real Lynx frame without waiting on the JS app to drive
// LynxWindow.loadURL. The product path stays JS-driven (Br W -> LynxWindow);
// this is a controlled bring-up path to prove the render pipe end to end.
//
// Safe to call multiple times (surface created/changed); only the first call
// builds and loads. Posts the work onto the UI thread task runner.
void LynxtronStartLynxBringup(void* window, int width, int height);

// Delivers an XComponent touch packet to the currently active LynxView (the
// view owned by the focused Harmony window). The active view is maintained by
// SetHarmonyActiveLynxView below.
void DispatchHarmonyLynxTouch(int phase, double x, double y, int32_t id);

// Sets the active LynxView for HarmonyOS input routing. This should track the
// focused window: set on focus and cleared on blur.
void SetHarmonyActiveLynxView(LynxView* view);

}  // namespace lynxtron

#endif  // SHELL_APP_LYNX_BRINGUP_HARMONY_H_
