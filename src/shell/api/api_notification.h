// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_API_NOTIFICATION_H_
#define LYNXTRON_SHELL_API_API_NOTIFICATION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "shell/api/event_emitter_mixin.h"
#include "shell/api/notifications/notification.h"
#include "shell/api/notifications/notification_delegate.h"
#include "shell/common/gin_converters/value_converter.h"
#include "shell/common/gin_helper/constructible.h"
#include "shell/common/gin_helper/wrappable.h"

namespace gin_helper {
class ErrorThrower;
}

namespace lynxtron {
namespace api {

struct NotificationActionStruct {
  std::string type;
  std::string text;
  std::string action;

  NotificationActionStruct();
  ~NotificationActionStruct();
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
  std::vector<NotificationActionStruct> actions;
  gfx::Image icon;

  NotificationOptions();
  ~NotificationOptions();
};

class Notification : public gin_helper::DeprecatedWrappable<Notification>,
                     public gin_helper::EventEmitterMixin<Notification>,
                     public gin_helper::Constructible<Notification>,
                     public NotificationDelegate {
 public:
  static Notification* New(gin_helper::ErrorThrower thrower,
                           std::optional<base::Value::Dict> options);

  static void FillObjectTemplate(v8::Isolate*, v8::Local<v8::ObjectTemplate>);
  static const char* GetClassName() { return "Notification"; }

  // gin_helper::DeprecatedWrappable
  static gin::DeprecatedWrapperInfo kWrapperInfo;
  const char* GetTypeName() override;

  // NotificationDelegate:
  void NotificationClick() override;
  void NotificationClosed() override;
  void NotificationDisplayed() override;
  void NotificationDestroyed() override;
  void NotificationFailed() override;
  void NotificationAction(unsigned index) override;
  void NotificationReplied(const std::string& reply) override;

  // JS Properties
  std::string GetTitle() const;
  std::string GetSubtitle() const;
  std::string GetBody() const;
  bool GetSilent() const;
  bool GetHasReply() const;
  std::string GetReplyPlaceholder() const;
  std::string GetSound() const;
  std::string GetUrgency() const;
  std::string GetCloseButtonText() const;
  std::string GetToastXml() const;
  std::vector<NotificationActionStruct> GetActions() const;

 protected:
  Notification(std::optional<base::Value::Dict> options);
  ~Notification() override;

  void Show();
  void Close();

 private:
  std::unique_ptr<lynxtron::Notification> notification_;
};

}  // namespace api
}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_API_NOTIFICATION_H_
