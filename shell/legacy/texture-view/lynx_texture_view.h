// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_LEGACY_TEXTURE_VIEW_LYNX_TEXTURE_VIEW_H_
#define LYNXTRON_SHELL_LEGACY_TEXTURE_VIEW_LYNX_TEXTURE_VIEW_H_

#include "platform/embedder/public/lynx_native_view.h"

namespace lynxtron {
namespace legacy {

class LynxTextureView : public lynx::pub::LynxNativeView {
 public:
  LynxTextureView();
  ~LynxTextureView() override;

  bool OnCreate() override;
  void OnDestroy() override;
  void OnAttach() override;
  void OnDetach() override;
  void OnLayoutChanged(float left,
                       float top,
                       float width,
                       float height,
                       float pixel_ratio) override;
  bool IsScrollEnabled() override { return true; }
  bool IsSurfaceEnabled() override { return true; }
  lynx_surface_buffer_mode_t SurfaceBufferMode() override { return mode_; }
  void OnPropertiesChanged(const lynx::pub::LynxValue& attrs,
                           const lynx::pub::LynxValue& events) override;

  void OnSurfaceAcquire(int32_t& pid,
                        int64_t& surface_id,
                        int32_t& width,
                        int32_t& height);
  void OnSwapBuffer();

 private:
  int width_ = 0;
  int height_ = 0;
  lynx_surface_buffer_mode_t mode_ = kDoubleBuffer;
};

}  // namespace legacy
}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_LEGACY_TEXTURE_VIEW_LYNX_TEXTURE_VIEW_H_
