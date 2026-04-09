// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/lynx_update_meta.h"

#include <memory>

#include "lynx/platform/embedder/public/lynx_template_data.h"
#include "lynx/platform/embedder/public/lynx_update_meta.h"

namespace lynxtron {

std::shared_ptr<lynx::pub::LynxUpdateMeta> LynxUpdateMeta::BuildCore() const {
  auto meta = std::make_shared<lynx::pub::LynxUpdateMeta>();
  meta->SetUpdateData(
      std::make_shared<lynx::pub::LynxTemplateData>(update_data_));
  meta->SetGlobalProps(
      std::make_shared<lynx::pub::LynxTemplateData>(global_props_));
  return meta;
}

}  // namespace lynxtron
