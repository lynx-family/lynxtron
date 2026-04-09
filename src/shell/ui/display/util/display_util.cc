// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "ui/display/util/display_util.h"

#include <stddef.h>

#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"

namespace display {

namespace {

base::flat_set<int64_t>* internal_display_ids() {
  static base::NoDestructor<base::flat_set<int64_t>> display_ids;
  return display_ids.get();
};

// A list of bogus sizes in mm that should be ignored.
// See crbug.com/136533. The first element maintains the minimum
// size required to be valid size.
const int kInvalidDisplaySizeList[][2] = {
    {40, 30},
    {50, 40},
    {160, 90},
    {160, 100},
};

}  // namespace

bool IsDisplaySizeValid(const gfx::Size& physical_size) {
  // Ignore if the reported display is smaller than minimum size.
  if (physical_size.width() <= kInvalidDisplaySizeList[0][0] ||
      physical_size.height() <= kInvalidDisplaySizeList[0][1]) {
    VLOG(1) << "Smaller than minimum display size";
    return false;
  }
  for (size_t i = 1; i < std::size(kInvalidDisplaySizeList); ++i) {
    const gfx::Size size(kInvalidDisplaySizeList[i][0],
                         kInvalidDisplaySizeList[i][1]);
    if (physical_size == size) {
      VLOG(1) << "Invalid display size detected:" << size.ToString();
      return false;
    }
  }
  return true;
}

int64_t GenerateDisplayID(uint16_t manufacturer_id,
                          uint32_t product_code_hash,
                          uint8_t output_index) {
  return ((static_cast<int64_t>(manufacturer_id) << 40) |
          (static_cast<int64_t>(product_code_hash) << 8) | output_index);
}

bool CompareDisplayIds(int64_t id1, int64_t id2) {
  if (id1 == id2) {
    return false;
  }
  // Output index is stored in the first 8 bits. See GetDisplayIdFromEDID
  // in edid_parser.cc.
  int index_1 = id1 & 0xFF;
  int index_2 = id2 & 0xFF;
  DCHECK_NE(index_1, index_2) << id1 << " and " << id2;
  bool first_is_internal = IsInternalDisplayId(id1);
  bool second_is_internal = IsInternalDisplayId(id2);
  if (first_is_internal && !second_is_internal) {
    return true;
  }
  if (!first_is_internal && second_is_internal) {
    return false;
  }
  return index_1 < index_2;
}

bool IsInternalDisplayId(int64_t display_id) {
  return base::Contains(*internal_display_ids(), display_id);
}

const base::flat_set<int64_t>& GetInternalDisplayIds() {
  return *internal_display_ids();
}

// static
bool HasInternalDisplay() {
  return !GetInternalDisplayIds().empty();
}

void SetInternalDisplayIds(base::flat_set<int64_t> display_ids) {
  *internal_display_ids() = std::move(display_ids);
}

// gfx::ColorSpace ForcedColorProfileStringToColorSpace(const std::string&
// value) {
//   if (value == "srgb")
//     return gfx::ColorSpace::CreateSRGB();
//   if (value == "display-p3-d65")
//     return gfx::ColorSpace::CreateDisplayP3D65();
//   if (value == "scrgb-linear")
//     return gfx::ColorSpace::CreateSRGBLinear();
//   if (value == "hdr10")
//     return gfx::ColorSpace::CreateHDR10();
//   if (value == "extended-srgb")
//     return gfx::ColorSpace::CreateExtendedSRGB();
//   if (value == "generic-rgb") {
//     return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::APPLE_GENERIC_RGB,
//                            gfx::ColorSpace::TransferID::GAMMA18);
//   }
//   if (value == "color-spin-gamma24") {
//     // Run this color profile through an ICC profile. The resulting color
//     space
//     // is slightly different from the input color space, and removing the ICC
//     // profile would require rebaselineing many layout tests.
//     gfx::ColorSpace color_space(
//         gfx::ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN,
//         gfx::ColorSpace::TransferID::GAMMA24);
//     return gfx::ICCProfile::FromColorSpace(color_space).GetColorSpace();
//   }
//   LOG(ERROR) << "Invalid forced color profile: \"" << value << "\"";
//   return gfx::ColorSpace::CreateSRGB();
// }

// gfx::ColorSpace GetForcedDisplayColorProfile() {
//   DCHECK(HasForceDisplayColorProfile());
//   std::string value =
//       base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
//           /*switches::kForceDisplayColorProfile=*/"force-color-profile");
//   return ForcedColorProfileStringToColorSpace(value);
// }

// bool HasForceDisplayColorProfile() {
//   return base::CommandLine::ForCurrentProcess()->HasSwitch(
//       /*switches::kForceDisplayColorProfile=*/"force-color-profile");
// }

#if BUILDFLAG(IS_CHROMEOS)
// Constructs the raster DisplayColorSpaces out of |snapshot_color_space|,
// including the HDR ones if present and |allow_high_bit_depth| is set.
gfx::DisplayColorSpaces CreateDisplayColorSpaces(
    const gfx::ColorSpace& snapshot_color_space,
    bool allow_high_bit_depth,
    const absl::optional<gfx::HDRStaticMetadata>& hdr_static_metadata) {
  // if (HasForceDisplayColorProfile()) {
  //   return gfx::DisplayColorSpaces(GetForcedDisplayColorProfile(),
  //                                  DisplaySnapshot::PrimaryFormat());
  // }

  // ChromeOS VMs (e.g. amd64-generic or betty) have INVALID Primaries; just
  // pass the color space along.
  if (!snapshot_color_space.IsValid()) {
    return gfx::DisplayColorSpaces(snapshot_color_space,
                                   DisplaySnapshot::PrimaryFormat());
  }

  const auto primary_id = snapshot_color_space.GetPrimaryID();

  skcms_Matrix3x3 primary_matrix{};
  if (primary_id == gfx::ColorSpace::PrimaryID::CUSTOM) {
    snapshot_color_space.GetPrimaryMatrix(&primary_matrix);
  }

  // Reconstruct the native colorspace with an IEC61966 2.1 transfer function
  // for SDR content (matching that of sRGB).
  gfx::ColorSpace sdr_color_space;
  if (primary_id == gfx::ColorSpace::PrimaryID::CUSTOM) {
    sdr_color_space = gfx::ColorSpace::CreateCustom(
        primary_matrix, gfx::ColorSpace::TransferID::SRGB);
  } else {
    sdr_color_space =
        gfx::ColorSpace(primary_id, gfx::ColorSpace::TransferID::SRGB);
  }
  gfx::DisplayColorSpaces display_color_spaces = gfx::DisplayColorSpaces(
      sdr_color_space, DisplaySnapshot::PrimaryFormat());

  if (allow_high_bit_depth && snapshot_color_space.IsHDR()) {
    gfx::ColorSpace hdr_color_space;
    if (primary_id == gfx::ColorSpace::PrimaryID::CUSTOM) {
      hdr_color_space = gfx::ColorSpace::CreatePiecewiseHDR(
          primary_id, display::kSDRJoint, display::kHDRLevel, &primary_matrix);
    } else {
      hdr_color_space = gfx::ColorSpace::CreatePiecewiseHDR(
          primary_id, display::kSDRJoint, display::kHDRLevel);
    }

    display_color_spaces.SetOutputColorSpaceAndBufferFormat(
        gfx::ContentColorUsage::kHDR, false /* needs_alpha */, hdr_color_space,
        gfx::BufferFormat::RGBA_1010102);
    display_color_spaces.SetOutputColorSpaceAndBufferFormat(
        gfx::ContentColorUsage::kHDR, true /* needs_alpha */, hdr_color_space,
        gfx::BufferFormat::RGBA_1010102);

    // TODO(https://crbug.com/1286074): Populate maximum luminance based on
    // `hdr_static_metadata`. For now, assume that the HDR maximum luminance
    // is 1,000% of the SDR maximum luminance.
    constexpr float kHDRMaxLuminanceRelative = 10.f;
    display_color_spaces.SetHDRMaxLuminanceRelative(kHDRMaxLuminanceRelative);
  }
  return display_color_spaces;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace display
