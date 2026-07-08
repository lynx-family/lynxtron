// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/icon_manager.h"

#include "ui/gfx/image/image.h"

namespace lynxtron {

gfx::Image LoadPlatformIcon(const base::FilePath& path,
                            IconManager::IconSize size,
                            float scale_factor) {
  return gfx::Image();
}

}  // namespace lynxtron
