// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/lynx_view.h"

#include <memory>
#include <string>
#include <utility>

#include "api/lynx_view/lynx_view_client.h"
#include "base/memory/weak_ptr.h"
#include "gfx/geometry/rect.h"
#include "lynx/platform/embedder/public/capi/lynx_env_capi.h"
#include "lynx/platform/embedder/public/lynx_template_bundle.h"
#include "shell/api/lynx_view/lynx_update_meta.h"
#include "shell/api/lynx_view/lynx_view_impl.h"
#include "shell/api/lynx_view/module/lynx_node_module.h"

namespace lynxtron {

LynxView::LynxView() = default;

// static
std::unique_ptr<LynxView> LynxView::Create(std::unique_ptr<LynxViewImpl> impl) {
  auto view = std::unique_ptr<LynxView>(new LynxView());
  view->impl_ = std::move(impl);
  return view;
}

LynxView::~LynxView() = default;

void LynxView::SetNodePlatformEnv(void* platform) {
  lynx_env_set_node_platform(platform);
  SetNodePlatformEnvToLynxNodeModule(platform);
}

void LynxView::LoadFile(const std::string& path,
                        const std::string& data,
                        const std::string& global_props) {
  impl_->LoadFile(path, data, global_props);
}

void LynxView::LoadURL(const std::string& url,
                       const std::string& data,
                       const std::string& global_props) {
  impl_->LoadURL(url, data, global_props);
}

void LynxView::LoadBundle(std::shared_ptr<lynx::pub::LynxTemplateBundle> bundle,
                          const std::string& data,
                          const std::string& global_props) {
  impl_->LoadBundle(std::move(bundle), data, global_props);
}

void LynxView::SetClient(base::WeakPtr<lynxtron::LynxViewClient> client) {
  impl_->SetClient(client);
}

void LynxView::SetBounds(const gfx::Rect& bounds) {
  impl_->SetBounds(bounds);
}

void LynxView::Focus() {
  impl_->Focus();
}

void LynxView::Close() {
  impl_->Close();
}

void LynxView::SendGlobalEvent(const std::string& event,
                               const std::string& json) {
  impl_->SendGlobalEvent(event, json);
}

void LynxView::UpdateData(const std::string& data,
                          const std::string& global_props) {
  impl_->UpdateData(data, global_props);
}

void LynxView::UpdateData(std::shared_ptr<LynxUpdateMeta> meta) {
  impl_->UpdateData(std::move(meta));
}

void LynxView::ReloadTemplate(const std::string& data,
                              const std::string& global_props) {
  impl_->ReloadTemplate(data, global_props);
}

void LynxView::UpdateScreenMetrics(float width,
                                   float height,
                                   float device_pixel_ratio) {
  impl_->UpdateScreenMetrics(width, height, device_pixel_ratio);
}

void LynxView::SetFrame(float x, float y, float width, float height) {
  impl_->SetFrame(x, y, width, height);
}

std::string LynxView::DumpUITreeForCDP() {
  return impl_->DumpUITreeForCDP();
}

void* LynxView::GetNativeWindow() {
  return impl_->GetNativeWindow();
}

void LynxView::EnterForeground() {
  impl_->EnterForeground();
}

void LynxView::EnterBackground() {
  impl_->EnterBackground();
}

}  // namespace lynxtron
