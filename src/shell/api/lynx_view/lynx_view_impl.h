// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_IMPL_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "lynx/platform/embedder/public/lynx_view_client.h"
#include "shell/api/lynx_view/lynx_view_client.h"
#include "shell/ui/gfx/geometry/rect.h"

namespace lynx {
namespace pub {
class LynxView;
class LynxTemplateBundle;
}  // namespace pub
}  // namespace lynx

namespace lynxtron {
class LynxViewBuilder;
class LynxUpdateMeta;

namespace api {
class LynxWindow;
}

class LynxViewImpl : public lynx::pub::LynxViewClient {
 public:
  LynxViewImpl() = default;
  ~LynxViewImpl() override;

  void Initialize(std::unique_ptr<lynx::pub::LynxView> core_view);
  void LoadFile(const std::string& path,
                const std::string& data,
                const std::string& global_props);
  void LoadURL(const std::string& url,
               const std::string& data,
               const std::string& global_props);
  void LoadBundle(std::shared_ptr<lynx::pub::LynxTemplateBundle> bundle,
                  const std::string& data,
                  const std::string& global_props);
  void SetClient(base::WeakPtr<lynxtron::LynxViewClient> client);
  void SetBounds(const gfx::Rect& bounds);
  void SendGlobalEvent(const std::string& event, const std::string& json);
  void ReloadTemplate(const std::string& data, const std::string& global_props);
  void UpdateData(std::shared_ptr<LynxUpdateMeta> meta);
  void UpdateData(const std::string& data, const std::string& global_props);
  void UpdateScreenMetrics(float width, float height, float device_pixel_ratio);
  void SetFrame(float x, float y, float width, float height);
  std::string DumpUITreeForCDP();
  void* GetNativeWindow();
  void Focus();
  void Close();
  void EnterForeground();
  void EnterBackground();

  // lynx::pub::LynxViewClient overrides
  void OnPageStart(const char* url) override;
  void OnLoadSuccess() override;
  void OnFirstScreen() override;
  void OnPageUpdated() override;
  void OnDataUpdated() override;
  void OnDestroy() override;
  void OnRuntimeReady() override;
  void OnReceivedError(int error_code, const char* message) override;
  void OnTimingSetup(const char* timing_info) override;
  void OnTimingUpdate(const char* timing_info,
                      const char* update_timing,
                      const char* update_flag) override;
  void OnEnterForeground() override;
  void OnEnterBackground() override;
  void OnFrameTiming(int64_t frame_start_time_in_ns,
                     int64_t frame_finish_time_in_ns) override;

 private:
  std::unique_ptr<lynx::pub::LynxView> lynx_view_;
  base::WeakPtr<lynxtron::LynxViewClient> lynx_view_client_;
  std::shared_ptr<LynxViewImpl> self_shared_ptr_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_IMPL_H_
