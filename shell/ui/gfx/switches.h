// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SWITCHES_H_
#define UI_GFX_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace switches {

extern const char kAnimationDurationScale[];
extern const char kDisableColorCorrectRendering[];

extern const char kDisableFontSubpixelPositioning[];
extern const char kEnableNativeGpuMemoryBuffers[];
extern const char kForcePrefersReducedMotion[];
extern const char kHeadless[];
}  // namespace switches

namespace features {
extern const base::Feature kOddHeightMultiPlanarBuffers;
}  // namespace features

#endif  // UI_GFX_SWITCHES_H_
