// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <memory>
#include <string>

#include "capi/lynx_export.h"
#include "lynx_extension_module.h"
#include "third_party/napi/include/primjs_napi_defines.h"

namespace extension {

class DemoExtensionModule : public lynx::pub::LynxExtensionModule {
 public:
  DemoExtensionModule() = default;
  ~DemoExtensionModule() override = default;

  void OnLynxViewCreate(lynx_view_t* lynx_view) override;
  void OnLynxViewDestroy() override;
  void OnRuntimeInit() override;
  void OnRuntimeAttach(
      napi_env env,
      std::unique_ptr<lynx::pub::VSyncObserver> vsync_observer) override;
  void OnRuntimeReady(napi_env env, napi_value lynx, const char* url) override;
  void OnRuntimeDetach() override;
  void OnEnterForeground() override;
  void OnEnterBackground() override;
  void Destroy() override;

  // js test
  std::string CallByLynxJS();
};

#include "third_party/napi/include/primjs_napi_undefs.h"

}  // namespace extension

LYNX_EXTERN_C_BEGIN
LYNX_CAPI_EXPORT lynx_extension_module_t*
demo_extension_module_create_extension_module(void* opaque);
LYNX_EXTERN_C_END
