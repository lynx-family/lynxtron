// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/lynx_view_builder.h"

#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "lynx/platform/embedder/public/lynx_view.h"
#include "shell/api/lynx_view/lynx_view.h"
#include "shell/api/lynx_view/lynx_view_impl.h"
#include "shell/api/lynx_view/module/lynx_bridge_module.h"
#include "shell/api/lynx_view/module/lynx_hybrid_monitor_module.h"
#include "shell/api/lynx_view/module/lynx_node_module.h"
#include "shell/legacy/texture-view/lynx_texture_view.h"
#include "shell/lynx/resource_fetcher/lynx_generic_resource_fetcher_factory.h"

namespace lynxtron {

struct LynxViewBuilder::Impl {
  lynx::pub::LynxView::Builder builder;
};

LynxViewBuilder::LynxViewBuilder() : impl_(std::make_unique<Impl>()) {}

LynxViewBuilder::~LynxViewBuilder() = default;

LynxViewBuilder& LynxViewBuilder::SetScreenSize(float width,
                                                float height,
                                                float pixel_ratio) {
  impl_->builder.SetScreenSize(width, height, pixel_ratio);
  return *this;
}

LynxViewBuilder& LynxViewBuilder::SetFrame(float x,
                                           float y,
                                           float width,
                                           float height) {
  impl_->builder.SetFrame(x, y, width, height);
  return *this;
}

LynxViewBuilder& LynxViewBuilder::SetICUDataPath(
    const std::string& icu_data_path) {
  impl_->builder.SetICUDataPath(icu_data_path);
  return *this;
}

LynxViewBuilder& LynxViewBuilder::SetParent(void* parent) {
  impl_->builder.SetParent(parent);
  return *this;
}

LynxViewBuilder& LynxViewBuilder::SetGenericResourceFetcher(
    std::shared_ptr<lynx::pub::LynxGenericResourceFetcher> fetcher) {
  impl_->builder.SetGenericResourceFetcher(fetcher);
  return *this;
}

LynxViewBuilder& LynxViewBuilder::SetLynxWindow(
    base::WeakPtr<api::LynxWindow> lynx_window) {
  lynx_window_ = lynx_window;
  return *this;
}

LynxViewBuilder& LynxViewBuilder::SetNodeIntegrationPreload(
    const std::vector<std::string>& preload) {
  node_integration_preload_ = preload;
  return *this;
}

std::unique_ptr<LynxView> LynxViewBuilder::Build() {
  base::FilePath icu_data_path;
  base::FilePath dir_path;
#if BUILDFLAG(IS_MAC)
  if (base::PathService::Get(base::DIR_ASSETS, &dir_path)) {
#else
  if (base::PathService::Get(base::DIR_MODULE, &dir_path)) {
#endif
    icu_data_path = dir_path.AppendASCII("icudtl.dat");
  }
  SetICUDataPath(icu_data_path.AsUTF8Unsafe());

  SetGenericResourceFetcher(
      LynxGenericResourceFetcherFactory::Create(lynx_window_));

  lynx_view_builder_register_native_view(
      impl_->builder.Impl(), "x-texture-view",
      [](void* opaque) -> lynx_native_view_t* {
        (void)opaque;
        return (new legacy::LynxTextureView())->native_view();
      },
      nullptr);

  if (!node_integration_preload_.empty()) {
    RegisterLynxNodeModuleToLynxView(impl_->builder.Impl(),
                                     node_integration_preload_);
  }
  RegisterLynxBridgeModuleToLynxView(impl_->builder.Impl(), lynx_window_);

  RegisterLynxHybridMonitorModuleToLynxView(impl_->builder.Impl(),
                                            lynx_window_);

  auto view_impl = std::make_unique<LynxViewImpl>();
  view_impl->Initialize(impl_->builder.Build());
  return LynxView::Create(std::move(view_impl));
}
}  // namespace lynxtron
