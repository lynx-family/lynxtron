// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_GFX_GEOMETRY_VECTOR2D_CONVERSIONS_H_
#define LYNXTRON_SHELL_UI_GFX_GEOMETRY_VECTOR2D_CONVERSIONS_H_

#include "base/component_export.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {

// Returns a Vector2d with each component from the input Vector2dF floored.
COMPONENT_EXPORT(GEOMETRY)
Vector2d ToFlooredVector2d(const Vector2dF& vector2d);

// Returns a Vector2d with each component from the input Vector2dF ceiled.
COMPONENT_EXPORT(GEOMETRY) Vector2d ToCeiledVector2d(const Vector2dF& vector2d);

// Returns a Vector2d with each component from the input Vector2dF rounded.
COMPONENT_EXPORT(GEOMETRY)
Vector2d ToRoundedVector2d(const Vector2dF& vector2d);

}  // namespace gfx

#endif  // LYNXTRON_SHELL_UI_GFX_GEOMETRY_VECTOR2D_CONVERSIONS_H_
