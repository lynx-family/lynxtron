// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_PROCESS_SINGLETON_CHROME_PROCESS_FINDER_H_
#define LYNXTRON_SHELL_COMMON_PROCESS_SINGLETON_CHROME_PROCESS_FINDER_H_

#include <windows.h>

#include "base/time/time.h"

namespace base {
class FilePath;
}

enum class NotifyChromeResult {
  NOTIFY_SUCCESS,
  NOTIFY_FAILED,
  NOTIFY_WINDOW_HUNG,
};

// Finds an already running Chrome window if it exists.
HWND FindRunningChromeWindow(const base::FilePath& user_data_dir);

// Attempts to send the current command line to an already running instance of
// Chrome via a WM_COPYDATA message.
// Returns true if a running Chrome is found and successfully notified.
NotifyChromeResult AttemptToNotifyRunningChrome(HWND remote_window);

// Changes the notification timeout to |new_timeout|, returns the old timeout.
base::TimeDelta SetNotificationTimeoutForTesting(base::TimeDelta new_timeout);

#endif  // LYNXTRON_SHELL_COMMON_PROCESS_SINGLETON_CHROME_PROCESS_FINDER_H_
