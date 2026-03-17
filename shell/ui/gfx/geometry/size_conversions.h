// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_GFX_GEOMETRY_SIZE_CONVERSIONS_H_
#define LYNXTRON_SHELL_UI_GFX_GEOMETRY_SIZE_CONVERSIONS_H_

#include "base/component_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace gfx {

// Returns a Size with each component from the input SizeF floored.
COMPONENT_EXPORT(GEOMETRY) Size ToFlooredSize(const SizeF& size);

// Returns a Size with each component from the input SizeF ceiled.
COMPONENT_EXPORT(GEOMETRY) Size ToCeiledSize(const SizeF& size);

// Returns a Size with each component from the input SizeF rounded.
COMPONENT_EXPORT(GEOMETRY) Size ToRoundedSize(const SizeF& size);

}  // namespace gfx

#endif  // LYNXTRON_SHELL_UI_GFX_GEOMETRY_SIZE_CONVERSIONS_H_
