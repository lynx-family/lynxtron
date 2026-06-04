// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/ui/gfx/mac/scoped_cocoa_disable_screen_updates.h"

#import <Cocoa/Cocoa.h>

namespace gfx {

ScopedCocoaDisableScreenUpdates::ScopedCocoaDisableScreenUpdates() {
  [NSAnimationContext beginGrouping];
}

ScopedCocoaDisableScreenUpdates::~ScopedCocoaDisableScreenUpdates() {
  [NSAnimationContext endGrouping];
}

}  // namespace gfx
