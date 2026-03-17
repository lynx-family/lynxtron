// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

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
