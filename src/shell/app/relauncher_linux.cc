// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/relauncher.h"

namespace relauncher::internal {

void RelauncherSynchronizeWithParent() {}

int LaunchProgram(const StringVector& relauncher_args,
                  const StringVector& argv) {
  return 1;
}

}  // namespace relauncher::internal
