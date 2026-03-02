// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_API_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_
#define LYNXTRON_SHELL_API_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_

#include <windows.h>

#include <wrl/client.h>

#include <memory>
#include <set>
#include <string>

#include "shell/api/notifications/platform_notification_service.h"

namespace lynxtron {

class Notification;

class NotificationPlatformBridgeWin : public PlatformNotificationService {
 public:
  NotificationPlatformBridgeWin();
  ~NotificationPlatformBridgeWin() override;

  // PlatformNotificationService:
  std::unique_ptr<Notification> CreateNotification(
      NotificationDelegate* delegate,
      NotificationOptions* options) override;

  // Returns the static instance of the bridge.
  static NotificationPlatformBridgeWin* Get();

  // Returns the set of currently active notifications.
  const std::set<Notification*>& notifications() const {
    return notifications_;
  }

  // Called by Notification when it is destroyed.
  void OnNotificationDestroyed(Notification* notification);

 private:
  bool Initialize();

  bool initialized_ = false;
  std::set<Notification*> notifications_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_
