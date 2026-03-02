// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_CENTER_DELEGATE_H_
#define LYNXTRON_SHELL_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_CENTER_DELEGATE_H_

#import <Foundation/Foundation.h>

namespace lynxtron {
class NotificationPlatformBridgeMac;
}

@interface NotificationCenterDelegate
    : NSObject <NSUserNotificationCenterDelegate> {
 @private
  lynxtron::NotificationPlatformBridgeMac* presenter_;
}

- (instancetype)initWithPresenter:
    (lynxtron::NotificationPlatformBridgeMac*)presenter;

@end

#endif  // LYNXTRON_SHELL_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_CENTER_DELEGATE_H_
