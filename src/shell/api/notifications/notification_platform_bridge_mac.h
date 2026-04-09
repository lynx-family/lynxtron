// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_
#define LYNXTRON_SHELL_API_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif

#include <memory>
#include <set>

#include "shell/api/notifications/platform_notification_service.h"

namespace lynxtron {

class MacNotification;

class NotificationPlatformBridgeMac : public PlatformNotificationService {
 public:
  NotificationPlatformBridgeMac();
  ~NotificationPlatformBridgeMac() override;

  // PlatformNotificationService:
  std::unique_ptr<Notification> CreateNotification(
      NotificationDelegate* delegate,
      NotificationOptions* options) override;

#ifdef __OBJC__
  MacNotification* GetNotification(NSUserNotification* ns_notification);
#endif

 private:
  friend class MacNotification;
  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_
