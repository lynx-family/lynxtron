// Copyright 2021 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/icon_manager.h"

#include <windows.h>

#include <combaseapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wrl/client.h>

#include "base/files/file_util.h"
#include "base/win/scoped_gdi_object.h"
#include "shell/ui/display/win/dpi.h"
#include "shell/ui/gfx/icon_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace lynxtron {
namespace {
gfx::Image LoadIconWithShGetFileInfo(const base::FilePath& path,
                                     IconManager::IconSize size) {
  base::FilePath normalized_path = path.NormalizePathSeparators();
  bool exists = base::PathExists(normalized_path);

  UINT flags = SHGFI_ICON;
  if (size == IconManager::IconSize::kSmall) {
    flags |= SHGFI_SMALLICON;

  } else if (size == IconManager::IconSize::kLarge) {
    flags |= SHGFI_LARGEICON;
  }
  if (!exists) {
    flags |= SHGFI_USEFILEATTRIBUTES;
  }

  SHFILEINFO file_info = {};
  if (SHGetFileInfoW(normalized_path.value().c_str(), FILE_ATTRIBUTE_NORMAL,
                     &file_info, sizeof(file_info), flags) == 0 ||
      !file_info.hIcon) {
    return gfx::Image();
  }

  base::win::ScopedGDIObject<HICON> icon(file_info.hIcon);
  SkBitmap skbitmap = IconUtil::CreateSkBitmapFromHICON(
      icon.get(), gfx::Size(IconManager::GetPixelSize(size),
                            IconManager::GetPixelSize(size)));
  if (skbitmap.isNull()) {
    return gfx::Image();
  }
  gfx::ImageSkia image_skia =
      gfx::ImageSkia::CreateFromBitmap(skbitmap, display::win::GetDPIScale());
  return gfx::Image(image_skia);
}

}  // namespace

gfx::Image LoadPlatformIcon(const base::FilePath& path,
                            IconManager::IconSize size,
                            float scale_factor) {
  return LoadIconWithShGetFileInfo(path, size);
}

}  // namespace lynxtron
