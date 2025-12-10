// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef SHELL_COMMON_PATH_PROVIDER_H_
#define SHELL_COMMON_PATH_PROVIDER_H_
namespace base {
class FilePath;
}
namespace lynxtron {
bool PathProvider(int key, base::FilePath* result);
}

#endif  // SHELL_COMMON_PATH_PROVIDER_H_
