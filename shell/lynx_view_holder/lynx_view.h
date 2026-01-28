// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_LYNX_VIEW_HOLDER_LYNX_VIEW_H_
#define LYNXTRON_SHELL_LYNX_VIEW_HOLDER_LYNX_VIEW_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "shell/lynx_view_holder/lynx_view_client.h"
#include "shell/ui/gfx/geometry/rect.h"

namespace lynxtron {
class LynxViewImpl;

class LynxView {
 public:
  static std::unique_ptr<LynxView> Create();
  ~LynxView();

  static void SetNodePlatformEnv(void* platform);

  void Init(double width,
            double height,
            float dpi,
            void* parent,
            bool node_integration);
  void LoadTemplate(std::string_view template_url, base::span<uint8_t> content);
  void SetClient(base::WeakPtr<LynxViewClient> client);
  void SetBounds(const gfx::Rect& bounds);
  void Show();
  void Hide();
  void Close();
  void SendGlobalEvent(const std::string& event, const std::string& json);
  void UpdateData(const std::string& data, const std::string& global_props);
  void ReloadTemplate(const std::string& data, const std::string& global_props);
  void UpdateScreenMetrics(float width, float height, float device_pixel_ratio);
  void SetFrame(float x, float y, float width, float height);
  void* GetNativeWindow();

 private:
  LynxView();

  std::shared_ptr<LynxViewImpl> impl_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_LYNX_VIEW_HOLDER_LYNX_VIEW_H_
