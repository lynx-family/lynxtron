// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include "shell/api/api_native_image.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "base/files/file_path.h"
#include "shell/common/gin_converters/image_converter.h"
#include "shell/common/gin_helper/promise.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
namespace lynxtron::api {
v8::Local<v8::Promise> NativeImage::CreateThumbnailFromPath(
    v8::Isolate* isolate,
    const base::FilePath& path,
    const gfx::Size& size) {
  gin_helper::Promise<gfx::Image> promise(isolate);
  auto handle = promise.GetHandle();
  if (size.IsEmpty()) {
    promise.RejectWithErrorMessage("Thumbnail size must be positive");
    return handle;
  }
  GError* error = nullptr;
  auto* pixbuf = gdk_pixbuf_new_from_file_at_scale(
      path.value().c_str(), size.width(), size.height(), TRUE, &error);
  if (!pixbuf) {
    promise.RejectWithErrorMessage(error ? error->message
                                         : "Cannot decode image");
    if (error) {
      g_error_free(error);
    }
    return handle;
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
  promise.Resolve(gfx::Image::CreateFrom1xBitmap(bitmap));
  return handle;
}
}  // namespace lynxtron::api
