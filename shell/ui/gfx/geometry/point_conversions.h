// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_GFX_GEOMETRY_POINT_CONVERSIONS_H_
#define LYNXTRON_SHELL_UI_GFX_GEOMETRY_POINT_CONVERSIONS_H_

#include "base/component_export.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

namespace gfx {

// Returns a Point with each component from the input PointF floored.
COMPONENT_EXPORT(GEOMETRY) Point ToFlooredPoint(const PointF& point);

// Returns a Point with each component from the input PointF ceiled.
COMPONENT_EXPORT(GEOMETRY) Point ToCeiledPoint(const PointF& point);

// Returns a Point with each component from the input PointF rounded.
COMPONENT_EXPORT(GEOMETRY) Point ToRoundedPoint(const PointF& point);

}  // namespace gfx

#endif  // LYNXTRON_SHELL_UI_GFX_GEOMETRY_POINT_CONVERSIONS_H_
