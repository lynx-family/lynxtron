// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "skia/ext/fontmgr_default.h"

#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace skia {

SK_API sk_sp<SkFontMgr> CreateDefaultSkFontMgr() {
  return SkFontMgr_New_DirectWrite();
}

}  // namespace skia
