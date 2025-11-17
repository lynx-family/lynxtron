// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE.chromium file.

#include "shell/common/gin_helper/locker.h"

#include "v8/include/v8-locker.h"

namespace gin_helper {

Locker::Locker(v8::Isolate* isolate) : locker_{new v8::Locker{isolate}} {}

Locker::~Locker() = default;

}  // namespace gin_helper
