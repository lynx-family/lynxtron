// Copyright (c) 2017 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_UI_COCOA_ELECTRON_BUNDLE_MOVER_H_
#define LYNXTRON_SHELL_UI_COCOA_ELECTRON_BUNDLE_MOVER_H_

#include "base/apple/foundation_util.h"

namespace gin {
class Arguments;
}

namespace gin_helper {
class ErrorThrower;
}  // namespace gin_helper

namespace lynxtron {

// Possible bundle movement conflicts
enum class BundlerMoverConflictType { kExists, kExistsAndRunning };

class ElectronBundleMover {
 public:
  static bool Move(gin_helper::ErrorThrower thrower, gin::Arguments* args);
  static bool IsCurrentAppInApplicationsFolder();

 private:
  static bool ShouldContinueMove(gin_helper::ErrorThrower thrower,
                                 BundlerMoverConflictType type,
                                 gin::Arguments* args);
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_UI_COCOA_ELECTRON_BUNDLE_MOVER_H_
