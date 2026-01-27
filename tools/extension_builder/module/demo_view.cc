// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "module/demo_view.h"

#include "capi/lynx_log_capi.h"

namespace extension {

void DemoView::OnPropertiesChanged(const lynx::pub::LynxValue& attrs,
                                   const lynx::pub::LynxValue& events) {
  if (attrs.HasProperty("demo-property")) {
    auto str = attrs.GetProperty("demo-property").StdString();
    LYNX_CAPI_LOG(LYNX_LOG_INFO, "TestTag", "Hello, %s!", str.c_str());
  }
}

}  // namespace extension

LYNX_EXTERN_C lynx_native_view_t* demo_view_create_view(void* opaque) {
  auto* view = new extension::DemoView();
  return view->native_view();
}
