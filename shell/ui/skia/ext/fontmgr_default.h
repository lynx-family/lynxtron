// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_SKIA_EXT_FONTMGR_DEFAULT_H_
#define LYNXTRON_SHELL_UI_SKIA_EXT_FONTMGR_DEFAULT_H_

#include "third_party/skia/include/core/SkTypes.h"

class SkFontMgr;
template <typename T>
class sk_sp;

namespace skia {

// Allows to override the default SkFontMgr instance (returned from
// SkFontMgr::RefDefault()). Must be called before RefDefault() is called for
// the first time in the process.
SK_API void OverrideDefaultSkFontMgr(sk_sp<SkFontMgr> fontmgr);

// Create default SkFontMgr implementation for the current platform.
SK_API sk_sp<SkFontMgr> CreateDefaultSkFontMgr();

}  // namespace skia

#endif  // LYNXTRON_SHELL_UI_SKIA_EXT_FONTMGR_DEFAULT_H_
