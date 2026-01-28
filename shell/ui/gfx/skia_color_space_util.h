// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_UI_GFX_SKIA_COLOR_SPACE_UTIL_H_
#define LYNXTRON_SHELL_UI_GFX_SKIA_COLOR_SPACE_UTIL_H_

#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkICC.h"
#include "third_party/skia/include/core/SkM44.h"

namespace gfx {

// Return the parameterized function in |fn|, evaluated at |x|. Note that this
// will clamp output values to the range [0, 1].
float SkTransferFnEval(const skcms_TransferFunction& fn, float x);

// Return the parameterized function in |fn|, evaluated at |x|. This will not
// clamp output values.
float SkTransferFnEvalUnclamped(const skcms_TransferFunction& fn, float x);

skcms_TransferFunction SkTransferFnInverse(const skcms_TransferFunction& fn);

skcms_TransferFunction SkTransferFnScaled(const skcms_TransferFunction& fn,
                                          float scale);

bool SkTransferFnsApproximatelyCancel(const skcms_TransferFunction& a,
                                      const skcms_TransferFunction& b);

bool SkTransferFnIsApproximatelyIdentity(const skcms_TransferFunction& fn);

bool SkM44IsApproximatelyIdentity(const SkM44& m);

SkM44 SkM44FromRowMajor3x3(const float* scale);

}  // namespace gfx

#endif  // LYNXTRON_SHELL_UI_GFX_SKIA_COLOR_SPACE_UTIL_H_
