// Copyright 2021 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/icon_manager.h"

#include <shellapi.h>
#include <shlobj.h>
#include <wrl/client.h>

#include "base/files/file_util.h"
#include "base/win/scoped_gdi_object.h"
#include "shell/ui/gfx/icon_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace lynxtron {
namespace {

gfx::Image LoadIconWithSysImageList(const base::FilePath& path,
                                    IconManager::IconSize size) {
  base::FilePath normalized_path = path.NormalizePathSeparators();
  bool exists = base::PathExists(normalized_path);
  DWORD attributes = base::DirectoryExists(normalized_path)
                         ? FILE_ATTRIBUTE_DIRECTORY
                         : FILE_ATTRIBUTE_NORMAL;

  SHFILEINFO file_info = {};
  UINT sys_flags = SHGFI_SYSICONINDEX;
  if (!exists) {
    sys_flags |= SHGFI_USEFILEATTRIBUTES;
  }
  if (SHGetFileInfoW(normalized_path.value().c_str(), attributes, &file_info,
                     sizeof(file_info), sys_flags) == 0) {
    return gfx::Image();
  }

  Microsoft::WRL::ComPtr<IImageList> image_list;
  HRESULT hr = SHGetImageList(SHIL_JUMBO, IID_PPV_ARGS(&image_list));
  if (FAILED(hr)) {
    hr = SHGetImageList(SHIL_EXTRALARGE, IID_PPV_ARGS(&image_list));
  }
  if (FAILED(hr) || !image_list) {
    return gfx::Image();
  }

  HICON hicon = nullptr;
  if (FAILED(image_list->GetIcon(file_info.iIcon, ILD_TRANSPARENT, &hicon)) ||
      !hicon) {
    return gfx::Image();
  }

  base::win::ScopedGDIObject<HICON> icon(hicon);
  SkBitmap skbitmap = IconUtil::CreateSkBitmapFromHICON(
      icon.get(), gfx::Size(IconManager::GetPixelSize(size),
                            IconManager::GetPixelSize(size)));
  if (skbitmap.isNull()) {
    return gfx::Image();
  }
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFromBitmap(skbitmap, 1.0f);
  return gfx::Image(image_skia);
}

gfx::Image LoadIconWithShGetFileInfo(const base::FilePath& path,
                                     IconManager::IconSize size) {
  base::FilePath normalized_path = path.NormalizePathSeparators();
  bool exists = base::PathExists(normalized_path);
  DWORD attributes = base::DirectoryExists(normalized_path)
                         ? FILE_ATTRIBUTE_DIRECTORY
                         : FILE_ATTRIBUTE_NORMAL;

  SHFILEINFO file_info = {};
  UINT flags = SHGFI_ICON;
  if (size == IconManager::IconSize::kSmall) {
    flags |= SHGFI_SMALLICON;
  } else {
    flags |= SHGFI_LARGEICON;
  }
  if (!exists) {
    flags |= SHGFI_USEFILEATTRIBUTES;
  }
  if (SHGetFileInfoW(normalized_path.value().c_str(), attributes, &file_info,
                     sizeof(file_info), flags) == 0 ||
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
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFromBitmap(skbitmap, 1.0f);
  return gfx::Image(image_skia);
}

}  // namespace

gfx::Image LoadPlatformIcon(const base::FilePath& path,
                            IconManager::IconSize size,
                            float scale_factor) {
  if (size == IconManager::IconSize::kLarge) {
    gfx::Image image = LoadIconWithSysImageList(path, size);
    if (!image.IsEmpty()) {
      return image;
    }
  }
  return LoadIconWithShGetFileInfo(path, size);
}

}  // namespace lynxtron
