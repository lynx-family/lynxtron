// Copyright (c) 2024 Microsoft, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_MAC_UTIL_H_
#define LYNXTRON_SHELL_COMMON_MAC_UTIL_H_

#include "base/containers/span.h"

@class NSData;

namespace lynxtron::util {

base::span<const uint8_t> as_byte_span(NSData* data);

}  // namespace lynxtron::util

#endif  // LYNXTRON_SHELL_COMMON_MAC_UTIL_H_
