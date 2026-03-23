// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/lynx_view.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "api/lynx_view/lynx_view_client.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "gfx/geometry/rect.h"
#include "lynx/platform/embedder/public/capi/lynx_env_capi.h"
#include "lynx/platform/embedder/public/lynx_view.h"
#include "shell/api/lynx_view/lynx_view_impl.h"
#include "shell/api/lynx_view/module/lynx_node_module.h"

// TODO(Guo Xi): review lynx view destroy process
namespace lynxtron {

LynxView::LynxView(base::WeakPtr<api::LynxWindow> lynx_window)
    : lynx_window_(lynx_window), impl_(std::make_unique<LynxViewImpl>()) {}

LynxView::~LynxView() = default;

std::unique_ptr<LynxView> LynxView::Create(
    base::WeakPtr<api::LynxWindow> lynx_window) {
  return base::WrapUnique(new LynxView(lynx_window));
}

// static
void LynxView::SetNodePlatformEnv(void* platform) {
  lynx_env_set_node_platform(platform);
  SetNodePlatformEnvToLynxNodeModule(platform);
}

void LynxView::Init(double width,
                    double height,
                    float dpi,
                    void* parent,
                    const std::vector<std::string>& node_integration_preload) {
  impl_->Init(width, height, dpi, parent, node_integration_preload,
              lynx_window_);
}

void LynxView::LoadTemplate(std::string_view template_url,
                            base::span<const uint8_t> content) {
  impl_->LoadTemplate(template_url, content);
}

void LynxView::SetClient(base::WeakPtr<lynxtron::LynxViewClient> client) {
  impl_->SetClient(client);
}

void LynxView::SetBounds(const gfx::Rect& bounds) {
  impl_->SetBounds(bounds);
}

void LynxView::Show() {
  impl_->Show();
}

void LynxView::Hide() {
  impl_->Hide();
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

void* LynxView::GetNativeWindow() {
  return impl_->GetNativeWindow();
}

}  // namespace lynxtron
