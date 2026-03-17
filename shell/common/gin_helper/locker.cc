// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE.chromium file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/gin_helper/locker.h"

#include "v8/include/v8-locker.h"

namespace gin_helper {

Locker::Locker(v8::Isolate* isolate) : locker_{new v8::Locker{isolate}} {}

Locker::~Locker() = default;

}  // namespace gin_helper
