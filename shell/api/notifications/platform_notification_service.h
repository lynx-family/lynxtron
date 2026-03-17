// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_H_
#define LYNXTRON_SHELL_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_H_

#include <memory>
#include <string>

#include "shell/api/notifications/notification.h"

namespace lynxtron {

class PlatformNotificationService {
 public:
  virtual ~PlatformNotificationService() {}

  static PlatformNotificationService* GetInstance();

  virtual std::unique_ptr<Notification> CreateNotification(
      NotificationDelegate* delegate,
      NotificationOptions* options) = 0;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_H_
