// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "ui/base/resource/resource_scale_factor.h"

#include <array>
#include <iterator>

namespace ui {

namespace {

const float kResourceScaleFactorScales[] = {1.0f, 1.0f, 2.0f, 3.0f};
static_assert(NUM_SCALE_FACTORS == std::size(kResourceScaleFactorScales),
              "kScaleFactorScales has incorrect size");

}  // namespace

float GetScaleForResourceScaleFactor(ResourceScaleFactor scale_factor) {
  return kResourceScaleFactorScales[scale_factor];
}

std::vector<ui::ResourceScaleFactor> GetSupportedResourceScaleFactors() {
  return {ui::k100Percent, ui::k200Percent};
}

ui::ResourceScaleFactor GetSupportedResourceScaleFactorForRescale(float scale) {
  ui::ResourceScaleFactor closest_match = ui::k100Percent;

  constexpr std::array<ui::ResourceScaleFactor, 2> scale_factors{
      ui::k100Percent, ui::k200Percent};

  const float kFallbackToSmallerScaleDiff = 0.20f;
  // Returns an exact match, a smaller scale within
  // `kFallbackToSmallerScaleDiff` units, the nearest larger scale, or the max
  // supported scale.
  for (auto supported_scale : scale_factors) {
    if (GetScaleForResourceScaleFactor(supported_scale) +
            kFallbackToSmallerScaleDiff >=
        scale) {
      return supported_scale;
    }
  }

  return ui::k200Percent;
}

}  // namespace ui
