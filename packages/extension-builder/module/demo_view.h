// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "lynxtron/lynx_native_view.h"

namespace extension {
class DemoView : public lynx::pub::LynxNativeView {
 public:
  explicit DemoView(void* user_data) {}

  bool IsSurfaceEnabled() override { return false; }

  void OnPropertiesChanged(const lynx::pub::LynxValue& attrs,
                           const lynx::pub::LynxValue& events) override;
};
}  // namespace extension

LYNX_EXTERN_C_BEGIN
lynx_native_view_t* demo_view_create_view(void* opaque);
LYNX_EXTERN_C_END
