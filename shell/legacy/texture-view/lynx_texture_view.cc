// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/legacy/texture-view/lynx_texture_view.h"

#include <utility>

#include "base/logging.h"
#include "build/build_config.h"
#include "platform/embedder/public/lynx_value.h"
#include "shell/plugin/lynx_plugin_module_manager.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_MAC)
#include <IOSurface/IOSurface.h>
#include <mach/mach.h>
#include <unistd.h>
#endif

using lynx::pub::LynxValue;

namespace lynxtron {
namespace legacy {

LynxTextureView::LynxTextureView() {
  LynxPluginModuleManager::RegisterSurfaceOperationCallback(
      [this]() { OnSwapBuffer(); },
      [this](int32_t& pid, int64_t& surface_id, int32_t& width,
             int32_t& height) {
        OnSurfaceAcquire(pid, surface_id, width, height);
      });
}

LynxTextureView::~LynxTextureView() {
  LOG(ERROR) << "LynxTextureView::~LynxTextureView";
  LynxPluginModuleManager::UnRegisterSurfaceOperationCallback();
}

void LynxTextureView::OnSwapBuffer() {
  SwapBack();
}

void LynxTextureView::OnDestroy() {
  LOG(ERROR) << "LynxTextureView::OnDestroy";
  TriggerEvent("destroy", LynxValue(LynxValue::kCreateAsNullTag));
}

bool LynxTextureView::OnCreate() {
  LOG(ERROR) << "LynxTextureView::OnCreate";
  TriggerEvent("create", LynxValue(LynxValue::kCreateAsNullTag));
  return true;
}

void LynxTextureView::OnAttach() {
  LOG(ERROR) << "LynxTextureView::OnAttach";
  TriggerEvent("attach", LynxValue(LynxValue::kCreateAsNullTag));
}

void LynxTextureView::OnDetach() {
  LOG(ERROR) << "LynxTextureView::OnDetach";
  TriggerEvent("detach", LynxValue(LynxValue::kCreateAsNullTag));
}

void LynxTextureView::OnLayoutChanged(float left,
                                      float top,
                                      float width,
                                      float height,
                                      float pixel_ratio) {
  LOG(ERROR) << "LynxTextureView::OnLayoutChanged " << width << " x " << height
             << " pixelRatio: " << pixel_ratio;
  if (width != 0 && height != 0) {
    width_ = width * pixel_ratio;
    height_ = height * pixel_ratio;

    LynxValue detail(LynxValue::kCreateAsMapTag);
    detail.SetProperty("pixelRatio", LynxValue(pixel_ratio));
    detail.SetProperty("width", LynxValue(width));
    detail.SetProperty("height", LynxValue(height));
    TriggerEvent("resize", std::move(detail));
  }
}

void LynxTextureView::OnSurfaceAcquire(int32_t& pid,
                                       int64_t& surface_id,
                                       int32_t& width,
                                       int32_t& height) {
  auto handle = AcquireSurface(width_, height_);
  if (!handle) {
    LOG(ERROR) << "LynxTextureView::OnSurfaceAcquire AcquireSurface failed";
    return;
  }

#if BUILDFLAG(IS_WIN)
  pid = GetCurrentProcessId();
  surface_id = int64_t(handle);
#elif BUILDFLAG(IS_MAC)
  pid = getpid();
  surface_id = IOSurfaceGetID((IOSurfaceRef)handle);
#endif
  width = width_;
  height = height_;

  LOG(ERROR) << "LynxTextureView::OnSurfaceAcquire pid: " << pid
             << " surface_id: " << surface_id << " width: " << width
             << " height: " << height;
}

void LynxTextureView::OnPropertiesChanged(const LynxValue& attrs,
                                          const LynxValue& events) {
  static const char* kBufferMode = "buffer-mode";
  if (attrs.HasProperty(kBufferMode)) {
    int buffer_mode = static_cast<int>(attrs.GetProperty(kBufferMode).Number());
    mode_ = static_cast<lynx_surface_buffer_mode_t>(buffer_mode);
    LOG(ERROR) << "LynxTextureView::OnPropertiesChanged buffer_mode: "
               << buffer_mode;
  }
}

}  // namespace legacy
}  // namespace lynxtron
