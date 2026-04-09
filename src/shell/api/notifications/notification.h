// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_NOTIFICATIONS_NOTIFICATION_H_
#define LYNXTRON_SHELL_API_NOTIFICATIONS_NOTIFICATION_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "shell/api/notifications/notification_delegate.h"
#include "ui/gfx/image/image.h"

namespace lynxtron {

struct NotificationAction {
  std::string type;
  std::string text;
  std::string action;

  NotificationAction();
  ~NotificationAction();
};

struct NotificationOptions {
  std::string title;
  std::string subtitle;
  std::string body;
  std::string tag;
  bool silent = false;
  bool has_reply = false;
  std::string reply_placeholder;
  std::string sound;
  std::string urgency;
  std::string close_button_text;
  std::string toast_xml;
  std::string timeout_type;  // Added this
  std::vector<NotificationAction> actions;
  gfx::Image icon;
  // std::string icon_url; // Electron has this

  NotificationOptions();
  ~NotificationOptions();
};

class Notification {
 public:
  Notification(NotificationDelegate* delegate, NotificationOptions* options);
  virtual ~Notification();

  // Shows the notification.
  virtual void Show();

  // Dismisses the notification.
  virtual void Dismiss();

  // The delegate of the notification.
  NotificationDelegate* delegate() const { return delegate_; }

  const NotificationOptions& options() const { return options_; }

  const std::string& notification_id() const { return notification_id_; }

  base::WeakPtr<Notification> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  NotificationDelegate* delegate_;
  NotificationOptions options_;
  std::string notification_id_;

 private:
  base::WeakPtrFactory<Notification> weak_factory_{this};

  Notification(const Notification&) = delete;
  Notification& operator=(const Notification&) = delete;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_NOTIFICATIONS_NOTIFICATION_H_
