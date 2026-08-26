// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/lynx/http_service/lynx_http_service.h"

#include <memory>

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "platform/embedder/http_service/lynx_http_service_impl.h"
#include "platform/embedder/public/lynx_service_center.h"
#endif

namespace lynxtron {

void RegisterLynxHttpService() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // The open-source build serves `fetch()` with the reusable Lynx embedder
  // HTTP service, whose transport is each platform's native networking stack
  // (NSURLSession on macOS, WinHTTP on Windows). Register it once with the
  // shared service center so LynxFetchModule can retrieve a non-null service.
  static const bool registered = [] {
    lynx::pub::LynxServiceCenter::GetInstance().RegisterService(
        std::make_shared<lynx::embedder::LynxHttpServiceImpl>());
    return true;
  }();
  static_cast<void>(registered);
#endif
}

}  // namespace lynxtron
