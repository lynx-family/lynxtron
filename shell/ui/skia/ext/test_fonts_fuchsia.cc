// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "skia/ext/test_fonts_fuchsia.h"

#include <fuchsia/fonts/cpp/fidl.h>

#include "skia/ext/fontmgr_default.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontMgr_fuchsia.h"

namespace skia {

void InitializeSkFontMgrForTest() {
  OverrideDefaultSkFontMgr(
      SkFontMgr_New_Fuchsia(GetTestFontsProvider().BindSync()));
}

}  // namespace skia
