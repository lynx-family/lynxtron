// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include "shell/app/icon_manager.h"

#include <gtk/gtk.h>

#include <cmath>

#include "shell/common/linux_desktop.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
namespace lynxtron {
gfx::Image LoadPlatformIcon(const base::FilePath& path,
                            IconManager::IconSize size,
                            float scale_factor) {
  if (!EnsureLinuxDesktop()) {
    return {};
  }
  auto* file = g_file_new_for_path(path.value().c_str());
  auto* info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_ICON,
                                 G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
  g_object_unref(file);
  if (!info) {
    return {};
  }
  auto* icon = gtk_icon_theme_lookup_by_gicon(
      gtk_icon_theme_get_default(), g_file_info_get_icon(info),
      std::ceil(IconManager::GetPixelSize(size) * scale_factor),
      GTK_ICON_LOOKUP_FORCE_SIZE);
  g_object_unref(info);
  if (!icon) {
    return {};
  }
  auto* pixbuf = gtk_icon_info_load_icon(icon, nullptr);
  g_object_unref(icon);
  if (!pixbuf) {
    return {};
  }
  SkBitmap bitmap;
  bitmap.allocN32Pixels(gdk_pixbuf_get_width(pixbuf),
                        gdk_pixbuf_get_height(pixbuf));
  int stride = gdk_pixbuf_get_rowstride(pixbuf),
      channels = gdk_pixbuf_get_n_channels(pixbuf);
  auto* pixels = gdk_pixbuf_get_pixels(pixbuf);
  for (int y = 0; y < bitmap.height(); ++y) {
    for (int x = 0; x < bitmap.width(); ++x) {
      auto* p = pixels + y * stride + x * channels;
      *bitmap.getAddr32(x, y) =
          SkPreMultiplyARGB(channels == 4 ? p[3] : 255, p[0], p[1], p[2]);
    }
  }
  g_object_unref(pixbuf);
  return gfx::Image(gfx::ImageSkia::CreateFromBitmap(bitmap, scale_factor));
}
}  // namespace lynxtron
