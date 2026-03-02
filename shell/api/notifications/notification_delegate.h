// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_BROWSER_NOTIFICATIONS_NOTIFICATION_DELEGATE_H_
#define LYNXTRON_SHELL_BROWSER_NOTIFICATIONS_NOTIFICATION_DELEGATE_H_

#include <string>

namespace lynxtron {

class NotificationDelegate {
 public:
  virtual ~NotificationDelegate() {}

  virtual void NotificationClick() = 0;
  virtual void NotificationClosed() = 0;
  virtual void NotificationDisplayed() = 0;
  virtual void NotificationDestroyed() = 0;
  virtual void NotificationFailed() = 0;
  virtual void NotificationAction(unsigned index) = 0;
  virtual void NotificationReplied(const std::string& reply) = 0;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_BROWSER_NOTIFICATIONS_NOTIFICATION_DELEGATE_H_
