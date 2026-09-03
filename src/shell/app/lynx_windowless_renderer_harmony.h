// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SHELL_APP_LYNX_WINDOWLESS_RENDERER_HARMONY_H_
#define SHELL_APP_LYNX_WINDOWLESS_RENDERER_HARMONY_H_

#include <cstdint>
#include <memory>

#include "lynx/platform/embedder/public/lynx_windowless_renderer.h"

namespace lynxtron {

// Render bridge: a GLDirect windowless renderer whose GL callbacks drive an
// EGL context created on the HarmonyOS XComponent surface. In multi-window
// mode each surface is tracked by its HarmonyOS window id so that renderer,
// surface size, and input stay per-window instead of being global singletons.
//
// Ownership model: the XComponent surface arrives asynchronously (ETS onLoad),
// while the LynxView is built later when the JS app creates it. We keep the
// renderer for each surface in a per-window map keyed by harmony_window_id;
// the LynxView build path fetches the renderer for its own window id and calls
// Builder::SetWindowlessRenderer().

// Creates (or returns the existing) windowless renderer bound to `egl_window`
// for the given HarmonyOS window id. Also records it as the renderer for that
// window. Safe to call again for the same window; recreates if the window
// changed.
std::shared_ptr<lynx::pub::LynxWindowlessRenderer>
CreateHarmonyWindowlessRenderer(int32_t harmony_window_id,
                                void* egl_window,
                                int width,
                                int height);

// Returns the renderer bound to the given HarmonyOS window id, or nullptr if
// no surface has arrived for that window yet. Used by the LynxView build path
// in multi-window mode.
std::shared_ptr<lynx::pub::LynxWindowlessRenderer>
GetHarmonyWindowlessRendererForWindow(int32_t harmony_window_id);

// Returns the renderer bound to the most recent XComponent surface, or nullptr
// if no surface has arrived yet. This is the legacy single-window fallback.
std::shared_ptr<lynx::pub::LynxWindowlessRenderer>
GetCurrentHarmonyWindowlessRenderer();

// Called while LynxView is being built. Captures the Clay platform sequence so
// host input can be dispatched on the same sequence later.
void CaptureHarmonyLynxPlatformTaskRunner();

// Returns the XComponent surface size for the given HarmonyOS window id into
// *w,*h and true, or false if no surface has arrived for that window yet.
bool GetHarmonySurfaceSizeForWindow(int32_t harmony_window_id, int* w, int* h);

// Returns the most recent XComponent surface size (physical px) into *w,*h and
// true, or false if no surface has arrived yet. NativeWindowHarmony uses this so
// the LynxWindow's LynxView is sized to the real surface, not a fixed default.
// This is the legacy single-window fallback.
bool GetCurrentHarmonySurfaceSize(int* w, int* h);

// Returns a placeholder windowless renderer for the given HarmonyOS window id.
// The placeholder forwards all renderer callbacks to a real renderer once the
// XComponent surface arrives and CreateHarmonyWindowlessRenderer() binds it.
// This lets a LynxView be built before its surface callback fires without
// falling back to the wrong window's renderer in multi-window mode.
std::shared_ptr<lynx::pub::LynxWindowlessRenderer>
GetOrCreateHarmonyPlaceholderRendererForWindow(int32_t harmony_window_id);

// Returns a placeholder windowless renderer keyed by the C++-allocated window
// id. Used when a LynxView is built before the ArkTS side has bound the
// HarmonyOS window id; BindHarmonyWindowIdForPlaceholder() re-keys it once the
// HarmonyOS id is known so the surface callback can bind the real renderer.
std::shared_ptr<lynx::pub::LynxWindowlessRenderer>
GetOrCreateHarmonyPlaceholderRendererForCppWindow(int32_t cpp_window_id);

// Re-keys a cpp-keyed placeholder renderer under its HarmonyOS window id. Safe
// to call when no placeholder exists. Invoked from LynxtronOnHarmonyWindowCreated
// when the ArkTS side finishes binding a window.
void BindHarmonyWindowIdForPlaceholder(int32_t cpp_window_id,
                                       int32_t harmony_window_id);

// Releases all per-window renderer/surface/placeholder state for a closed
// window. Invoked from NativeWindowHarmony's destructor so a late event routed
// to the old harmony id cannot reach a dangling renderer.
void ReleaseHarmonyWindowRenderer(int32_t cpp_window_id,
                                  int32_t harmony_window_id);

// Snapshot of the cached Lynx IME request and caret rectangle for the given
// HarmonyOS window id. This is used by the NAPI bridge on the ArkUI UI
// sequence; no renderer object escapes.
bool GetHarmonyTextInputStateForWindow(int32_t harmony_window_id,
                                       float* x,
                                       float* y,
                                       float* width,
                                       float* height);

// Snapshot of the cached Lynx IME request and caret rectangle. Legacy
// single-window fallback.
bool GetCurrentHarmonyTextInputState(float* x,
                                     float* y,
                                     float* width,
                                     float* height);

// Callback signature used by the NAPI bridge to observe text-input focus/caret
// changes per HarmonyOS window id. The bridge updates the global IME target
// window and moves the candidate window accordingly.
using HarmonyTextInputFocusCallback =
    void (*)(int32_t harmony_window_id,
             bool visible,
             float x,
             float y,
             float width,
             float height);

// Registers the text-input focus callback. Harmony-only; called once from the
// NAPI bridge during initialization.
void LynxtronSetHarmonyTextInputFocusCallback(
    HarmonyTextInputFocusCallback callback);

// Called from surface_render_harmony.cc when the XComponent surface arrives.
// Deprecated: use LynxtronSetHarmonySurfaceSizeForWindow with an explicit id.
extern "C" void LynxtronSetHarmonySurfaceSize(int width, int height);

}  // namespace lynxtron

#endif  // SHELL_APP_LYNX_WINDOWLESS_RENDERER_HARMONY_H_
