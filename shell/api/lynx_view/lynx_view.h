// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "shell/api/lynx_view/lynx_view_client.h"
#include "shell/api/lynx_view/lynx_view_impl.h"
#include "shell/ui/gfx/geometry/rect.h"

namespace lynxtron {
namespace api {
class LynxWindow;
}

class LynxView {
 public:
  static std::unique_ptr<LynxView> Create(
      base::WeakPtr<api::LynxWindow> lynx_window);

  ~LynxView();

  static void SetNodePlatformEnv(void* platform);

  void Init(double width,
            double height,
            float dpi,
            void* parent,
            const std::vector<std::string>& node_integration_preload);
  void LoadTemplate(std::string_view template_url,
                    base::span<const uint8_t> content);
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
  explicit LynxView(base::WeakPtr<api::LynxWindow> lynx_window);
  base::WeakPtr<api::LynxWindow> lynx_window_;
  std::unique_ptr<LynxViewImpl> impl_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_H_
