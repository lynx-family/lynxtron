// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_native_image.h"

namespace lynxtron::api {

// HarmonyOS fallback for NativeImage::CreateThumbnailFromPath, which
// is declared in api_native_image.h:89 with no #if guard but defined only
// in api_native_image_mac.mm:67 (uses QuickLookThumbnailing) and
// api_native_image_win.cc:27 (uses IShellItemImageFactory). Returns an
// empty v8::Local<v8::Promise> so callers see "thumbnail not available"
// in the JS layer. A native implementation can use OHOS resource thumbnail
// APIs.
v8::Local<v8::Promise> NativeImage::CreateThumbnailFromPath(
    v8::Isolate* isolate,
    const base::FilePath& path,
    const gfx::Size& size) {
  return v8::Local<v8::Promise>();
}

}  // namespace lynxtron::api
