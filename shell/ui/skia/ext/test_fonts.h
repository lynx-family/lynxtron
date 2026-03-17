// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_SKIA_EXT_TEST_FONTS_H_
#define LYNXTRON_SHELL_UI_SKIA_EXT_TEST_FONTS_H_

namespace skia {

// Configures the process to use //third_party/test_fonts. Should be called
// early, before default instance of SkFontMgr is created.
void InitializeSkFontMgrForTest();

}  // namespace skia

#endif  // LYNXTRON_SHELL_UI_SKIA_EXT_TEST_FONTS_H_
