// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/icon_manager.h"

#include "base/files/file_path.h"
#include "ui/gfx/image/image.h"

namespace lynxtron {

// HarmonyOS fallback for the LoadPlatformIcon free function
// forward-declared in icon_manager.cc:16 and defined only in
// icon_manager_mac.mm:14 and icon_manager_win.cc:61. The cross-platform
// IconManager::LoadIcon (icon_manager.cc:91/96/104) takes its address and
// invokes it via thread-pool, so harmony must provide a definition or link
// fails.
//
// Returns an empty gfx::Image: IconManager::OnIconLoaded (icon_manager.cc:113)
// already short-circuits on `icon.IsEmpty()` and the user-facing callback
// receives an empty image - the same observable behaviour callers already
// handle when the file path doesn't exist on the supported platforms.
//
// A native implementation can use the OHOS bundle resource manager to extract
// module-bundled icons.
gfx::Image LoadPlatformIcon(const base::FilePath& path,
                            IconManager::IconSize size,
                            float scale_factor) {
  return gfx::Image();
}

}  // namespace lynxtron
