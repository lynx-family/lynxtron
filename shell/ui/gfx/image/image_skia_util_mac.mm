// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "ui/gfx/image/image_skia_util_mac.h"

#import <AppKit/AppKit.h>
#include <stddef.h>

#include <cmath>
#include <limits>
#include <memory>

#include "base/mac/mac_util.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
namespace {

// Returns true if the NSImage has no representations.
bool IsNSImageEmpty(NSImage* image) {
  return image.representations.count == 0;
}

std::vector<ui::ResourceScaleFactor> GetSupportedResourceScaleFactors() {
  return {ui::k100Percent, ui::k200Percent};
}

}  // namespace

namespace gfx {

gfx::ImageSkia ImageSkiaFromNSImage(NSImage* image) {
  return ImageSkiaFromResizedNSImage(image, image.size);
}

gfx::ImageSkia ImageSkiaFromResizedNSImage(NSImage* image,
                                           NSSize desired_size) {
  gfx::ImageSkia image_skia;
  const std::vector<ui::ResourceScaleFactor>& supported_scales =
      ui::GetSupportedResourceScaleFactors();
  for (const auto resource_scale : supported_scales) {
    const float scale = ui::GetScaleForResourceScaleFactor(resource_scale);
    NSAffineTransform* transform = [NSAffineTransform transform];
    [transform scaleBy:scale];
    NSDictionary<NSImageHintKey, id>* hints = @{NSImageHintCTM : transform};
    NSImageRep* best_match =
        [image bestRepresentationForRect:{NSZeroPoint, desired_size}
                                 context:NULL
                                   hints:hints];
    NSSize desired_size_for_scale =
        NSMakeSize(desired_size.width * scale, desired_size.height * scale);
    SkBitmap bitmap(skia::NSImageRepToSkBitmapWithColorSpace(
        best_match, desired_size_for_scale, false,
        base::mac::GetSRGBColorSpace()));
    if (bitmap.isNull()) {
      continue;
    }

    image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
  }

  return image_skia;
}

NSImage* NSImageFromImageSkia(const gfx::ImageSkia& image_skia) {
  return NSImageFromImageSkiaWithColorSpace(image_skia,
                                            base::mac::GetSRGBColorSpace());
}

NSImage* NSImageFromImageSkiaWithColorSpace(const gfx::ImageSkia& image_skia,
                                            CGColorSpaceRef color_space) {
  if (image_skia.isNull()) {
    return nil;
  }
  NSImage* image = [[NSImage alloc] init];
  image_skia.EnsureRepsForSupportedScales();
  std::vector<gfx::ImageSkiaRep> image_reps = image_skia.image_reps();
  for (const auto& rep : image_reps) {
    [image addRepresentation:skia::SkBitmapToNSBitmapImageRepWithColorSpace(
                                 rep.GetBitmap(), color_space)];
  }
  [image setSize:NSMakeSize(image_skia.width(), image_skia.height())];
  return image;
}

}  // namespace gfx
