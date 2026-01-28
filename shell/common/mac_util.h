// Copyright (c) 2024 Microsoft, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_COMMON_MAC_UTIL_H_
#define LYNXTRON_SHELL_COMMON_MAC_UTIL_H_

#include "base/containers/span.h"

@class NSData;

namespace lynxtron::util {

base::span<const uint8_t> as_byte_span(NSData* data);

}  // namespace lynxtron::util

#endif  // LYNXTRON_SHELL_COMMON_MAC_UTIL_H_
