// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_power_monitor.h"

#include <wtsapi32.h>

#include "base/functional/bind.h"
#include "shell/ui/base/win/session_change_observer.h"

namespace lynxtron::api {

void PowerMonitor::InitPlatformSpecificMonitors() {
  session_change_observer_ =
      std::make_unique<ui::SessionChangeObserver>(base::BindRepeating(
          &PowerMonitor::OnSessionChange, base::Unretained(this)));
}

void PowerMonitor::ShutdownPlatformSpecificMonitors() {
  session_change_observer_.reset();
}

void PowerMonitor::OnSessionChange(WPARAM wparam,
                                   const bool* is_current_session) {
  if (is_current_session && !*is_current_session) {
    return;
  }

  switch (wparam) {
    case WTS_SESSION_LOCK:
      Emit("lock-screen");
      break;
    case WTS_SESSION_UNLOCK:
      Emit("unlock-screen");
      break;
    default:
      break;
  }
}

}  // namespace lynxtron::api
