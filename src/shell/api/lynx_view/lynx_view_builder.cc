// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/lynx_view_builder.h"

#include <unordered_map>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "lynx/platform/embedder/public/lynx_view.h"
#if BUILDFLAG(IS_LINUX)
#include "lynx/platform/embedder/public/lynx_windowless_renderer.h"
#endif
#include "shell/api/lynx_view/lynx_view.h"
#include "shell/api/lynx_view/lynx_view_impl.h"
#include "shell/api/lynx_view/module/lynx_bridge_module.h"
#include "shell/api/lynx_view/module/lynx_hybrid_monitor_module.h"
#include "shell/api/lynx_view/module/lynx_node_module.h"
#include "shell/common/global_thread.h"
#include "shell/lynx/resource_fetcher/lynx_generic_resource_fetcher_factory.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#endif

namespace lynxtron {
namespace {

#if BUILDFLAG(IS_LINUX)
class LinuxWindowlessRenderer : public lynx::pub::LynxWindowlessRenderer {
 public:
  LinuxWindowlessRenderer()
      : lynx::pub::LynxWindowlessRenderer(kRendererTypeSoftware) {}

  bool OnSoftwarePresent(const void* allocation,
                         size_t row_bytes,
                         size_t height) override {
    return true;
  }

  void OnPostTask(lynx_task_t task, uint64_t interval_nanoseconds) override {
    std::weak_ptr<lynx::pub::LynxWindowlessRenderer> renderer =
        weak_from_this();
    GlobalThread::GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](std::weak_ptr<lynx::pub::LynxWindowlessRenderer> renderer,
               lynx_task_t task) {
              if (auto locked_renderer = renderer.lock()) {
                locked_renderer->RunTask(task);
              }
            },
            std::move(renderer), task),
        base::Nanoseconds(interval_nanoseconds));
  }
};
#endif

}  // namespace

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

LynxViewBuilder& LynxViewBuilder::SetNativeViewCreator(
    const char* name,
    lynx_native_view_creator creator,
    void* opaque) {
  if (!name || !*name) {
    LOG(ERROR) << "Invalid native view creator name: " << name;
    return *this;
  }
  lynx_view_builder_register_native_view(impl_->builder.Impl(), name, creator,
                                         opaque);
  return *this;
}

LynxViewBuilder& LynxViewBuilder::SetWebView2FixedRuntimePath(
    const char* path) {
  if (!path || !*path) {
    LOG(ERROR) << "Invalid webview2 fixed runtime path: " << path;
    return *this;
  }
#if BUILDFLAG(IS_WIN)
  impl_->builder.SetWebView2FixedRuntimePath(path);
#endif
  return *this;
}

std::unique_ptr<LynxView> LynxViewBuilder::Build() {
  base::FilePath icu_data_path;
#if BUILDFLAG(IS_MAC)
  icu_data_path = base::apple::PathForFrameworkBundleResource("icudtl.dat");
#else
  base::FilePath dir_path;
  if (base::PathService::Get(base::DIR_MODULE, &dir_path)) {
    icu_data_path = dir_path.AppendASCII("icudtl.dat");
  }
#endif
  SetICUDataPath(icu_data_path.AsUTF8Unsafe());

  SetGenericResourceFetcher(
      LynxGenericResourceFetcherFactory::Create(lynx_window_));

#if BUILDFLAG(IS_LINUX)
  impl_->builder.SetWindowlessRenderer(
      std::make_shared<LinuxWindowlessRenderer>());
#endif

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
