// Copyright (c) 2020 Slack Technologies, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_PLATFORM_UTIL_INTERNAL_H_
#define LYNXTRON_SHELL_COMMON_PLATFORM_UTIL_INTERNAL_H_

#include <string>

#include "shell/common/platform_util.h"

namespace base {
class FilePath;
}

namespace platform_util::internal {

// Called by platform_util.cc on to invoke platform specific logic to move
// |path| to trash using a suitable handler.
bool PlatformTrashItem(const base::FilePath& path, std::string* error);

}  // namespace platform_util::internal

#endif  // LYNXTRON_SHELL_COMMON_PLATFORM_UTIL_INTERNAL_H_
