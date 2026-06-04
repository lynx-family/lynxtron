// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "ui/gfx/geometry/insets_outsets_f_base.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/outsets_f.h"

namespace gfx {

template <typename T>
std::string InsetsOutsetsFBase<T>::ToString() const {
  return base::StringPrintf("x:%g,%g y:%g,%g", left_, right_, top_, bottom_);
}

template class InsetsOutsetsFBase<InsetsF>;
template class InsetsOutsetsFBase<OutsetsF>;

}  // namespace gfx
