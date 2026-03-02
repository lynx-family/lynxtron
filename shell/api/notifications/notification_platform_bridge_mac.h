// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_
#define LYNXTRON_SHELL_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_

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

#endif  // LYNXTRON_SHELL_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_
