// Copyright 2021 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/icon_manager.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"
#include "ui/gfx/image/image.h"

namespace lynxtron {

gfx::Image LoadPlatformIcon(const base::FilePath& path,
                            IconManager::IconSize size,
                            float scale_factor) {
  base::FilePath normalized_path = path.NormalizePathSeparators();
  NSString* path_string = base::SysUTF8ToNSString(normalized_path.value());
  if (!path_string) {
    return gfx::Image();
  }

  NSImage* icon = [[NSWorkspace sharedWorkspace] iconForFile:path_string];
  if (!icon) {
    return gfx::Image();
  }

  int pixel_size = IconManager::GetPixelSize(size);
  [icon setSize:NSMakeSize(pixel_size, pixel_size)];
  gfx::Image image(icon);
  return image;
}

}  // namespace lynxtron
