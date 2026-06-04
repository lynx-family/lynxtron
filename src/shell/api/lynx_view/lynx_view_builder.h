// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_BUILDER_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_BUILDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "lynx/platform/embedder/public/capi/lynx_native_view_capi.h"

namespace lynx {
namespace pub {
class LynxView;
class LynxGenericResourceFetcher;
}  // namespace pub
}  // namespace lynx

namespace lynxtron {
namespace api {
class LynxWindow;
}
class LynxView;

class LynxViewBuilder {
 public:
  LynxViewBuilder();
  ~LynxViewBuilder();

  LynxViewBuilder& SetScreenSize(float width, float height, float pixel_ratio);
  LynxViewBuilder& SetFrame(float x, float y, float width, float height);
  LynxViewBuilder& SetICUDataPath(const std::string& icu_data_path);
  LynxViewBuilder& SetParent(void* parent);
  LynxViewBuilder& SetGenericResourceFetcher(
      std::shared_ptr<lynx::pub::LynxGenericResourceFetcher> fetcher);

  LynxViewBuilder& SetLynxWindow(base::WeakPtr<api::LynxWindow> lynx_window);
  LynxViewBuilder& SetNodeIntegrationPreload(
      const std::vector<std::string>& preload);
  LynxViewBuilder& SetNativeViewCreator(const char* name,
                                        lynx_native_view_creator creator,
                                        void* opaque = nullptr);
  LynxViewBuilder& SetWebView2FixedRuntimePath(const char* path);

  std::unique_ptr<LynxView> Build();

  LynxViewBuilder(const LynxViewBuilder&) = delete;
  LynxViewBuilder& operator=(const LynxViewBuilder&) = delete;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  base::WeakPtr<api::LynxWindow> lynx_window_;
  std::vector<std::string> node_integration_preload_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_BUILDER_H_
