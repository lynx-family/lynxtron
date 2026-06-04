// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_GFX_GEOMETRY_TRIANGLE_F_H_
#define LYNXTRON_SHELL_UI_GFX_GEOMETRY_TRIANGLE_F_H_

#include "base/component_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace gfx {

COMPONENT_EXPORT(GEOMETRY)
bool PointIsInTriangle(const PointF& point,
                       const PointF& r1,
                       const PointF& r2,
                       const PointF& r3);
}  // namespace gfx

#endif  // LYNXTRON_SHELL_UI_GFX_GEOMETRY_TRIANGLE_F_H_
