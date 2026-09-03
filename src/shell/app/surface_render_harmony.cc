// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// HarmonyOS surface rendering bridge.
//
// The NAPI bridge (liblynxtron_napi.so) owns the XComponent lifecycle and
// hands us the raw OHNativeWindow via LynxtronSetNativeSurface(). This file
// lives in liblynxtron.so where Skia (Clay's renderer) is linked, sets up a
// Skia Ganesh GL context on that surface, and draws a frame. This proves the
// Skia -> XComponent surface path end to end — the same path Clay's Lynx UI
// rendering will use once the Lynx embedder is un-stubbed.

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstdint>
#include <hilog/log.h>

#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTypeface.h"
#include "include/gpu/GrBackendSurface.h"
#include "include/gpu/GrDirectContext.h"
#include "include/gpu/gl/GrGLAssembleInterface.h"
#include "include/gpu/gl/GrGLInterface.h"
#include "include/gpu/gl/GrGLTypes.h"
#include "shell/app/native_window_harmony.h"
#include "shell/app/lynx_windowless_renderer_harmony.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0000
#define LOG_TAG "LynxtronSurface"
#define SR_LOG(fmt, ...) OH_LOG_INFO(LOG_APP, "[SkSurf] " fmt, ##__VA_ARGS__)
#define SR_ERR(fmt, ...) OH_LOG_ERROR(LOG_APP, "[SkSurf] " fmt, ##__VA_ARGS__)

namespace {

EGLDisplay g_display = EGL_NO_DISPLAY;
EGLSurface g_surface = EGL_NO_SURFACE;
EGLContext g_context = EGL_NO_CONTEXT;
sk_sp<GrDirectContext> g_gr_context;

// Creates the EGL display/config/surface/context on the calling thread.
bool SetupEgl(void* window) {
  g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (g_display == EGL_NO_DISPLAY) {
    SR_ERR("eglGetDisplay failed");
    return false;
  }
  EGLint major = 0, minor = 0;
  if (!eglInitialize(g_display, &major, &minor)) {
    SR_ERR("eglInitialize failed 0x%{public}x", eglGetError());
    return false;
  }
  SR_LOG("EGL %{public}d.%{public}d", major, minor);

  const EGLint cfg_attrs[] = {EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
                              EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                              EGL_RED_SIZE,        8,
                              EGL_GREEN_SIZE,      8,
                              EGL_BLUE_SIZE,       8,
                              EGL_ALPHA_SIZE,      8,
                              EGL_DEPTH_SIZE,      0,
                              EGL_STENCIL_SIZE,    8,
                              EGL_NONE};
  EGLConfig cfg;
  EGLint num_cfg = 0;
  if (!eglChooseConfig(g_display, cfg_attrs, &cfg, 1, &num_cfg) ||
      num_cfg < 1) {
    SR_ERR("eglChooseConfig failed");
    return false;
  }

  g_surface = eglCreateWindowSurface(
      g_display, cfg, reinterpret_cast<EGLNativeWindowType>(window), nullptr);
  if (g_surface == EGL_NO_SURFACE) {
    SR_ERR("eglCreateWindowSurface failed 0x%{public}x", eglGetError());
    return false;
  }

  const EGLint ctx_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  g_context =
      eglCreateContext(g_display, cfg, EGL_NO_CONTEXT, ctx_attrs);
  if (g_context == EGL_NO_CONTEXT) {
    SR_ERR("eglCreateContext failed");
    return false;
  }
  eglMakeCurrent(g_display, g_surface, g_surface, g_context);
  return true;
}

// Draws one Skia demo frame (light background, centered card, accent bar)
// onto the EGL window surface and presents it.
void DrawFrame(int width, int height) {
  if (!g_gr_context) {
    auto interface = GrGLMakeAssembledGLESInterface(
        nullptr, [](void*, const char* name) -> GrGLFuncPtr {
          return reinterpret_cast<GrGLFuncPtr>(eglGetProcAddress(name));
        });
    if (!interface) {
      SR_ERR("GrGLMakeAssembledGLESInterface failed");
      return;
    }
    g_gr_context = GrDirectContext::MakeGL(interface);
    if (!g_gr_context) {
      SR_ERR("GrDirectContext::MakeGL failed");
      return;
    }
    SR_LOG("GrDirectContext created");
  }

  GrGLFramebufferInfo fb_info;
  fb_info.fFBOID = 0;  // default framebuffer (the EGL window surface)
  fb_info.fFormat = GL_RGBA8;

  GrBackendRenderTarget backend_rt(width, height, /*sampleCnt=*/0,
                                   /*stencilBits=*/8, fb_info);

  sk_sp<SkSurface> surface = SkSurface::MakeFromBackendRenderTarget(
      g_gr_context.get(), backend_rt, kBottomLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, nullptr, nullptr);
  if (!surface) {
    SR_ERR("MakeFromBackendRenderTarget failed (%{public}dx%{public}d)",
           width, height);
    return;
  }

  SkCanvas* canvas = surface->getCanvas();
  // Light background so it's visibly different from the bridge's blue clear.
  canvas->clear(SkColorSetRGB(0xF5, 0xF7, 0xFA));

  // A Lynxtron-blue rounded card in the center.
  SkPaint card;
  card.setAntiAlias(true);
  card.setColor(SkColorSetRGB(0x0D, 0x52, 0xD9));
  float cw = width * 0.6f, ch = height * 0.3f;
  float cx = (width - cw) / 2, cy = (height - ch) / 2;
  canvas->drawRoundRect(SkRect::MakeXYWH(cx, cy, cw, ch), 24, 24, card);

  // Accent bar.
  SkPaint bar;
  bar.setAntiAlias(true);
  bar.setColor(SkColorSetRGB(0x4C, 0xC2, 0xFF));
  canvas->drawRoundRect(
      SkRect::MakeXYWH(cx + 40, cy + 40, cw - 80, 20), 10, 10, bar);

  g_gr_context->flushAndSubmit();
  eglSwapBuffers(g_display, g_surface);
  SR_LOG("Skia frame drawn %{public}dx%{public}d", width, height);
}

}  // namespace

// Exported for the NAPI bridge (dlsym'd from liblynxtron_napi.so's
// OnSurfaceCreated). Sets up Skia GL on the XComponent surface and draws.
// __attribute__((visibility("default"))) keeps it in the dynamic symbol
// table despite -fvisibility=hidden so dlsym can find it.
extern "C" __attribute__((visibility("default"))) void
LynxtronSetNativeSurface(int32_t harmony_window_id,
                         void* window,
                         int width,
                         int height) {
  SR_LOG("LynxtronSetNativeSurface harmony_id=%{public}d window=%{public}p "
         "%{public}dx%{public}d",
         harmony_window_id, window, width, height);
  if (!window) {
    SR_ERR("null window");
    return;
  }
  // Register the GLDirect windowless renderer for this surface so the LynxView
  // build path (lynx_view_builder harmony branch) drives Clay onto it. This is
  // now the SINGLE EGL owner of the XComponent window — the former Skia test
  // card created a second EGL window surface on the same OHNativeWindow, which
  // made Clay's OnGLMakeCurrent fail with EGL_BAD_ACCESS (0x3002). The card is
  // removed now that Clay renders the real Lynx bundle here.
  lynxtron::CreateHarmonyWindowlessRenderer(harmony_window_id, window, width, height);
  // The same OHNativeWindow is retained across a full-screen transition, so
  // tell the existing NativeWindow/LynxView about every size change. Without
  // this its viewport stays at the prior size and pointer coordinates hit-test
  // at a different scale from the ArkUI XComponent.
  lynxtron::UpdateHarmonyNativeWindowSizeForWindow(harmony_window_id, width, height);

  // Sync the native window's bounds to the actual surface size so the LynxView
  // viewport matches the render target. With an explicit harmony_window_id this
  // works for multi-window; if the id is invalid we fall back to single-window
  // behavior inside the callee.
  lynxtron::LynxtronSetHarmonySurfaceSizeForWindow(harmony_window_id, width, height);

  // NOTE: no bring-up LynxView here anymore. The default_app JS creates a real
  // LynxWindow (BrowserWindow) and loads its own Lynx app; that window's
  // LynxView drives this renderer. Creating a second bring-up view here made two
  // LynxViews fight over the one surface (an 800x600 one and a 2090x1293 one).
}
