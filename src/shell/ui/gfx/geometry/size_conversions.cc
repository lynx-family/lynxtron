// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "ui/gfx/geometry/size_conversions.h"

#include "base/numerics/safe_conversions.h"

namespace gfx {

Size ToFlooredSize(const SizeF& size) {
  return Size(base::ClampFloor(size.width()), base::ClampFloor(size.height()));
}

Size ToCeiledSize(const SizeF& size) {
  return Size(base::ClampCeil(size.width()), base::ClampCeil(size.height()));
}

Size ToRoundedSize(const SizeF& size) {
  return Size(base::ClampRound(size.width()), base::ClampRound(size.height()));
}

}  // namespace gfx
