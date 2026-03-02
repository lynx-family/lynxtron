// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/api/notifications/notification_platform_bridge_win.h"

#include <memory>

#include "shell/api/notifications/windows_toast_activator.h"
#include "shell/api/notifications/windows_toast_notification.h"
#include "shell/common/application_info.h"

namespace lynxtron {

static NotificationPlatformBridgeWin* g_notification_presenter_win = nullptr;

NotificationPlatformBridgeWin::NotificationPlatformBridgeWin() {
  g_notification_presenter_win = this;
  Initialize();
}

NotificationPlatformBridgeWin::~NotificationPlatformBridgeWin() {
  g_notification_presenter_win = nullptr;
}

// static
NotificationPlatformBridgeWin* NotificationPlatformBridgeWin::Get() {
  return g_notification_presenter_win;
}

std::unique_ptr<Notification> NotificationPlatformBridgeWin::CreateNotification(
    NotificationDelegate* delegate,
    NotificationOptions* options) {
  if (!initialized_) {
    return nullptr;
  }
  auto notification =
      std::make_unique<WindowsToastNotification>(delegate, options);
  notifications_.insert(notification.get());
  return notification;
}

void NotificationPlatformBridgeWin::OnNotificationDestroyed(
    Notification* notification) {
  notifications_.erase(notification);
}

bool NotificationPlatformBridgeWin::Initialize() {
  if (!WindowsToastNotification::Initialize()) {
    return false;
  }

  NotificationActivator::RegisterActivator();

  initialized_ = true;
  return true;
}

}  // namespace lynxtron
