// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "shell/api/lynx_view/lynx_view_client.h"
#include "shell/ui/gfx/geometry/rect.h"

namespace lynx {
namespace pub {
class LynxTemplateBundle;
}  // namespace pub
}  // namespace lynx

namespace lynxtron {
class LynxViewBuilder;
class LynxViewImpl;
class LynxUpdateMeta;

class LynxView {
 public:
  ~LynxView();

  static void SetNodePlatformEnv(void* platform);
  void LoadFile(const std::string& path,
                const std::string& data,
                const std::string& global_props);
  void LoadURL(const std::string& url,
               const std::string& data,
               const std::string& global_props);
  void LoadBundle(std::shared_ptr<lynx::pub::LynxTemplateBundle> bundle,
                  const std::string& data,
                  const std::string& global_props);
  void SetClient(base::WeakPtr<LynxViewClient> client);
  void SetBounds(const gfx::Rect& bounds);
  void Focus();
  void Close();
  void SendGlobalEvent(const std::string& event, const std::string& json);
  void UpdateData(std::shared_ptr<LynxUpdateMeta> meta);
  void UpdateData(const std::string& data, const std::string& global_props);
  void ReloadTemplate(const std::string& data, const std::string& global_props);
  void UpdateScreenMetrics(float width, float height, float device_pixel_ratio);
  void SetFrame(float x, float y, float width, float height);
  void* GetNativeWindow();
  void EnterForeground();
  void EnterBackground();

 private:
  friend class LynxViewBuilder;

  LynxView();
  static std::unique_ptr<LynxView> Create(std::unique_ptr<LynxViewImpl> impl);

  std::unique_ptr<LynxViewImpl> impl_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_H_
