// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/notifications/notification.h"

#include "api/notifications/notification_delegate.h"
#include "base/uuid.h"

namespace lynxtron {

NotificationOptions::NotificationOptions() = default;
NotificationOptions::~NotificationOptions() = default;

NotificationAction::NotificationAction() = default;
NotificationAction::~NotificationAction() = default;

Notification::Notification(NotificationDelegate* delegate,
                           NotificationOptions* options)
    : delegate_(delegate) {
  if (options) {
    options_ = *options;
  }
  notification_id_ = base::Uuid::GenerateRandomV4().AsLowercaseString();
}

Notification::~Notification() {
  if (delegate_) {
    delegate_->NotificationDestroyed();
  }
}

void Notification::Show() {}

void Notification::Dismiss() {}

}  // namespace lynxtron
