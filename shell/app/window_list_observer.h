// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_APP_WINDOW_LIST_OBSERVER_H_
#define LYNXTRON_SHELL_APP_WINDOW_LIST_OBSERVER_H_

#include "base/observer_list_types.h"

namespace lynxtron {

class NativeWindow;

class WindowListObserver : public base::CheckedObserver {
 public:
  // Called when a window close is cancelled by beforeunload handler.
  virtual void OnWindowCloseCancelled(NativeWindow* window) {}

  // Called immediately after all windows are closed.
  virtual void OnWindowAllClosed() {}

 protected:
  ~WindowListObserver() override = default;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_WINDOW_LIST_OBSERVER_H_
