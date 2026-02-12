// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/lynx_view.h"

#include <string>
#include <string_view>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "shell/api/lynx_view/lynx_view_client.h"

#if BUILDFLAG(IS_MAC)
#if BUILD_WITH_LYNX
#include "lynx/platform/embedder/public/capi/lynx_env_capi.h"
#include "lynx/platform/embedder/public/lynx_view.h"
#include "lynx/platform/embedder/public/lynx_view_client.h"
#else
#include "shell/lynx/lynx_lib/lib/mac/include/capi/lynx_env_capi.h"
#include "shell/lynx/lynx_lib/lib/mac/include/lynx_view.h"
#include "shell/lynx/lynx_lib/lib/mac/include/lynx_view_client.h"
#endif
#endif

#if BUILDFLAG(IS_WIN)
#if BUILD_WITH_LYNX
#include "lynx/platform/embedder/public/capi/lynx_env_capi.h"
#include "lynx/platform/embedder/public/lynx_view.h"
#include "lynx/platform/embedder/public/lynx_view_client.h"
#else
#include "shell/lynx/lynx_lib/lib/win/include/capi/lynx_env_capi.h"
#include "shell/lynx/lynx_lib/lib/win/include/lynx_view.h"
#include "shell/lynx/lynx_lib/lib/win/include/lynx_view_client.h"
#endif
#endif

#include "shell/api/lynx_view/module/lynx_bridge_module.h"
#include "shell/api/lynx_view/module/lynx_node_module.h"

namespace lynxtron {

class LynxViewImpl : public lynx::pub::LynxViewClient,
                     public std::enable_shared_from_this<LynxViewImpl> {
 public:
  LynxViewImpl() = default;
  ~LynxViewImpl() = default;

  void Init(double width,
            double height,
            float dpi,
            void* parent,
            bool node_integration,
            base::WeakPtr<api::LynxWindow> lynx_window) {
    lynx::pub::LynxView::Builder builder;
    builder.SetScreenSize(width, height, dpi)
        .SetFrame(0, 0, width, height)
        .SetParent(parent);

    if (node_integration) {
      RegisterLynxNodeModuleToLynxView(builder.Impl());
    }
    RegisterLynxBridgeModuleToLynxView(builder.Impl(), lynx_window);

    lynx_view_ = builder.Build();
    lynx_view_->AddClient(shared_from_this());
  }

  void LoadTemplate(std::string_view template_url,
                    base::span<uint8_t> content) {
    auto meta_data = std::make_shared<lynx::pub::LynxLoadMeta>();
    meta_data->SetUrl(std::string(template_url));

    meta_data->SetBinaryData(content.data(), content.size());
    lynx_view_->LoadTemplate(meta_data);
  }

  void SetClient(base::WeakPtr<lynxtron::LynxViewClient> client) {
    lynx_view_client_ = client;
  }

  void SetBounds(const gfx::Rect& bounds) {
    lynx_view_->SetFrame(bounds.x(), bounds.y(), bounds.width(),
                         bounds.height());
  }

  void SendGlobalEvent(const std::string& event, const std::string& json) {
    lynx_view_->SendGlobalEvent(event, json);
  }

  void ReloadTemplate(const std::string& data,
                      const std::string& global_props) {
    lynx_view_->ReloadTemplate(
        std::make_shared<lynx::pub::LynxTemplateData>(data),
        std::make_shared<lynx::pub::LynxTemplateData>(global_props));
  }

  void UpdateData(const std::string& data, const std::string& global_props) {
    auto meta_data = std::make_shared<lynx::pub::LynxUpdateMeta>();
    meta_data->SetUpdateData(
        std::make_shared<lynx::pub::LynxTemplateData>(data));
    meta_data->SetGlobalProps(
        std::make_shared<lynx::pub::LynxTemplateData>(global_props));
    lynx_view_->UpdateData(meta_data);
  }

  void UpdateScreenMetrics(float width,
                           float height,
                           float device_pixel_ratio) {
    lynx_view_->UpdateScreenMetrics(width, height, device_pixel_ratio);
  }

  void SetFrame(float x, float y, float width, float height) {
    lynx_view_->SetFrame(x, y, width, height);
  }

  void* GetNativeWindow() {
    return lynx_view_ ? lynx_view_->GetNativeWindow() : nullptr;
  }

  void Show() {}

  void Hide() {}

  void Close() { lynx_view_.reset(); }

  // TODO(Guo Xi): check lynx callback thread
  // lynx::pub::LynxViewClient
  void OnPageStart(const char* url) override {
    if (lynx_view_client_) {
      lynx_view_client_->OnPageStart(url);
    }
  }
  void OnLoadSuccess() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnLoadSuccess();
    }
  }
  void OnFirstScreen() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnFirstScreen();
    }
  }
  void OnPageUpdated() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnPageUpdated();
    }
  }
  void OnDataUpdated() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnDataUpdated();
    }
  }
  void OnDestroy() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnDestroy();
    }
  }
  void OnRuntimeReady() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnRuntimeReady();
    }
  }
  void OnReceivedError(int error_code, const char* message) override {
    if (lynx_view_client_) {
      lynx_view_client_->OnReceivedError(error_code, message);
    }
  }
  void OnTimingSetup(const char* timing_info) override {
    if (lynx_view_client_) {
      lynx_view_client_->OnTimingSetup(timing_info);
    }
  }
  void OnTimingUpdate(const char* timing_info,
                      const char* update_timing,
                      const char* update_flag) override {
    if (lynx_view_client_) {
      lynx_view_client_->OnTimingUpdate(timing_info, update_timing,
                                        update_flag);
    }
  }
  void OnEnterForeground() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnEnterForeground();
    }
  }
  void OnEnterBackground() override {
    if (lynx_view_client_) {
      lynx_view_client_->OnEnterBackground();
    }
  }

 private:
  std::unique_ptr<lynx::pub::LynxView> lynx_view_;
  base::WeakPtr<lynxtron::LynxViewClient> lynx_view_client_;
};

LynxView::LynxView(base::WeakPtr<api::LynxWindow> lynx_window)
    : lynx_window_(lynx_window), impl_(std::make_shared<LynxViewImpl>()) {}

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
                    bool node_integration) {
  impl_->Init(width, height, dpi, parent, node_integration, lynx_window_);
}

void LynxView::LoadTemplate(std::string_view template_url,
                            base::span<uint8_t> content) {
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
