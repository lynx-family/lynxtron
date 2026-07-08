// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_native_image.h"

#include "shell/common/gin_helper/promise.h"
#include "ui/gfx/image/image.h"

namespace lynxtron::api {

// static
v8::Local<v8::Promise> NativeImage::CreateThumbnailFromPath(
    v8::Isolate* isolate,
    const base::FilePath& path,
    const gfx::Size& size) {
  gin_helper::Promise<gfx::Image> promise(isolate);
  v8::Local<v8::Promise> handle = promise.GetHandle();
  promise.RejectWithErrorMessage(
      "Creating thumbnails is not implemented on Linux");
  return handle;
}

}  // namespace lynxtron::api
