// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_GFX_MAC_SCOPED_COCOA_DISABLE_SCREEN_UPDATES_H_
#define LYNXTRON_SHELL_UI_GFX_MAC_SCOPED_COCOA_DISABLE_SCREEN_UPDATES_H_

#include "shell/ui/gfx/gfx_export.h"

namespace gfx {

// A stack-based class to disable Cocoa screen updates. When instantiated, it
// disables screen updates and enables them when destroyed. Update disabling
// can be nested, and there is a time-maximum (about 1 second) after which
// Cocoa will automatically re-enable updating. This class doesn't attempt to
// overrule that.
class GFX_EXPORT ScopedCocoaDisableScreenUpdates {
 public:
  ScopedCocoaDisableScreenUpdates();
  ~ScopedCocoaDisableScreenUpdates();
};

}  // namespace gfx

#endif  // LYNXTRON_SHELL_UI_GFX_MAC_SCOPED_COCOA_DISABLE_SCREEN_UPDATES_H_
