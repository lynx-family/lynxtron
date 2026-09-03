// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/relauncher.h"

#include "base/logging.h"

namespace relauncher {
namespace internal {

// HarmonyOS fallbacks for relauncher::internal free functions called
// unconditionally from the cross-platform relauncher.cc:
//   - relauncher.cc:171 calls RelauncherSynchronizeWithParent()
//   - relauncher.cc:196 calls LaunchProgram(relauncher_args, launch_argv)
//
// Both are declared in relauncher.h:97/99 with no #if guard, but defined
// only in relauncher_win.cc:118/133 and relauncher_mac.cc:25/81. Without
// these stubs, harmony build of lynxtron_lib hits two undefined references
// in the relauncher path. A native implementation can use a pipe-based parent
// watch and base::LaunchProcess.

void RelauncherSynchronizeWithParent() {
  // Parent synchronization is not currently implemented on HarmonyOS.
}

int LaunchProgram(const StringVector& relauncher_args,
                  const StringVector& argv) {
  // Stub: returns non-zero so callers (relauncher.cc:196) propagate
  // "failed to launch program" cleanly instead of falsely signalling
  // success without performing the relaunch.
  LOG(ERROR) << "relauncher::internal::LaunchProgram is not implemented on "
                "HarmonyOS";
  return 1;
}

}  // namespace internal
}  // namespace relauncher
