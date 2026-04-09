// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "ui/gfx/image/image_skia_source.h"

namespace gfx {

ImageSkiaSource::~ImageSkiaSource() {}

bool ImageSkiaSource::HasRepresentationAtAllScales() const {
  return false;
}

}  // namespace gfx
