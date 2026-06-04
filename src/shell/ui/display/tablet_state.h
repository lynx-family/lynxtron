// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_DISPLAY_TABLET_STATE_H_
#define LYNXTRON_SHELL_UI_DISPLAY_TABLET_STATE_H_

namespace display {

// Tracks whether we are in the process of entering or exiting tablet mode.
enum class TabletState {
  kInClamshellMode,
  kEnteringTabletMode,
  kInTabletMode,
  kExitingTabletMode,
};

}  // namespace display

#endif  // LYNXTRON_SHELL_UI_DISPLAY_TABLET_STATE_H_
