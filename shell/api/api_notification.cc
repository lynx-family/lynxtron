// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/api/api_notification.h"

#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "gin/function_template.h"
#include "shell/api/notifications/platform_notification_service.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/node_includes.h"

namespace gin {

template <>
struct Converter<lynxtron::api::NotificationActionStruct> {
  static v8::Local<v8::Value> ToV8(
      v8::Isolate* isolate,
      const lynxtron::api::NotificationActionStruct& val) {
    gin_helper::Dictionary dict = gin_helper::Dictionary::CreateEmpty(isolate);
    dict.Set("type", val.type);
    dict.Set("text", val.text);
    dict.Set("action", val.action);
    return dict.GetHandle();
  }
};

}  // namespace gin

namespace lynxtron {
namespace api {

gin::DeprecatedWrapperInfo Notification::kWrapperInfo = {
    gin::kEmbedderNativeGin};

NotificationActionStruct::NotificationActionStruct() = default;
NotificationActionStruct::~NotificationActionStruct() = default;

NotificationOptions::NotificationOptions() = default;
NotificationOptions::~NotificationOptions() = default;

Notification::Notification(std::optional<base::Value::Dict> options) {
  NotificationOptions opts;
  if (options) {
    if (const std::string* title = options->FindString("title")) {
      opts.title = *title;
    }
    if (const std::string* body = options->FindString("body")) {
      opts.body = *body;
    }
    if (const std::string* subtitle = options->FindString("subtitle")) {
      opts.subtitle = *subtitle;
    }
    if (std::optional<bool> silent = options->FindBool("silent")) {
      opts.silent = *silent;
    }
    if (const std::string* sound = options->FindString("sound")) {
      opts.sound = *sound;
    }
    if (std::optional<bool> has_reply = options->FindBool("hasReply")) {
      opts.has_reply = *has_reply;
    }
    if (const std::string* reply_placeholder =
            options->FindString("replyPlaceholder")) {
      opts.reply_placeholder = *reply_placeholder;
    }
    if (const std::string* urgency = options->FindString("urgency")) {
      opts.urgency = *urgency;
    }
    if (const std::string* close_button_text =
            options->FindString("closeButtonText")) {
      opts.close_button_text = *close_button_text;
    }
    if (const std::string* toast_xml = options->FindString("toastXml")) {
      opts.toast_xml = *toast_xml;
    }
    if (const base::Value::List* actions = options->FindList("actions")) {
      for (const auto& action : *actions) {
        if (action.is_dict()) {
          const auto& dict = action.GetDict();
          NotificationActionStruct action_obj;
          if (const std::string* type = dict.FindString("type")) {
            action_obj.type = *type;
          }
          if (const std::string* text = dict.FindString("text")) {
            action_obj.text = *text;
          }
          if (const std::string* act = dict.FindString("action")) {
            action_obj.action = *act;
          }
          opts.actions.push_back(std::move(action_obj));
        }
      }
    }

    lynxtron::NotificationOptions options_converted;
    options_converted.title = opts.title;
    options_converted.subtitle = opts.subtitle;
    options_converted.body = opts.body;
    options_converted.tag = opts.tag;
    options_converted.silent = opts.silent;
    options_converted.has_reply = opts.has_reply;
    options_converted.reply_placeholder = opts.reply_placeholder;
    options_converted.sound = opts.sound;
    options_converted.urgency = opts.urgency;
    options_converted.close_button_text = opts.close_button_text;
    options_converted.toast_xml = opts.toast_xml;
    options_converted.icon = opts.icon;
    for (const auto& action : opts.actions) {
      lynxtron::NotificationAction action_obj;
      action_obj.type = action.type;
      action_obj.text = action.text;
      action_obj.action = action.action;
      options_converted.actions.push_back(action_obj);
    }

    auto* service = PlatformNotificationService::GetInstance();
    if (service) {
      notification_ = service->CreateNotification(this, &options_converted);
    }
  } else {
    auto* service = PlatformNotificationService::GetInstance();
    if (service) {
      lynxtron::NotificationOptions options_converted;
      notification_ = service->CreateNotification(this, &options_converted);
    }
  }
}

Notification::~Notification() {
  if (notification_) {
    notification_->Dismiss();
  }
}

std::string Notification::GetTitle() const {
  return notification_ ? notification_->options().title : "";
}

std::string Notification::GetSubtitle() const {
  return notification_ ? notification_->options().subtitle : "";
}

std::string Notification::GetBody() const {
  return notification_ ? notification_->options().body : "";
}

bool Notification::GetSilent() const {
  return notification_ ? notification_->options().silent : false;
}

bool Notification::GetHasReply() const {
  return notification_ ? notification_->options().has_reply : false;
}

std::string Notification::GetReplyPlaceholder() const {
  return notification_ ? notification_->options().reply_placeholder : "";
}

std::string Notification::GetSound() const {
  return notification_ ? notification_->options().sound : "";
}

std::string Notification::GetUrgency() const {
  return notification_ ? notification_->options().urgency : "";
}

std::string Notification::GetCloseButtonText() const {
  return notification_ ? notification_->options().close_button_text : "";
}

std::string Notification::GetToastXml() const {
  return notification_ ? notification_->options().toast_xml : "";
}

std::vector<NotificationActionStruct> Notification::GetActions() const {
  std::vector<NotificationActionStruct> actions;
  if (notification_) {
    for (const auto& action : notification_->options().actions) {
      NotificationActionStruct action_struct;
      action_struct.type = action.type;
      action_struct.text = action.text;
      action_struct.action = action.action;
      actions.push_back(action_struct);
    }
  }
  return actions;
}

void Notification::Show() {
  if (notification_) {
    notification_->Show();
  }
}

void Notification::Close() {
  if (notification_) {
    notification_->Dismiss();
  }
}

void Notification::NotificationClick() {
  Emit("click");
}

void Notification::NotificationClosed() {
  Emit("close");
}

void Notification::NotificationDisplayed() {
  Emit("show");
}

void Notification::NotificationDestroyed() {
  // notification_ is being destroyed
}

void Notification::NotificationFailed() {
  Emit("failed", "Notification failed to show");
}

void Notification::NotificationAction(unsigned index) {
  if (index >= notification_->options().actions.size()) {
    return;
  }
  // action event should emit (event, action)
  Emit("action", notification_->options().actions[index].action);
}

void Notification::NotificationReplied(const std::string& reply) {
  Emit("reply", reply);
}

// static
Notification* Notification::New(gin_helper::ErrorThrower thrower,
                                std::optional<base::Value::Dict> options) {
  return new Notification(std::move(options));
}

// static
void Notification::FillObjectTemplate(v8::Isolate* isolate,
                                      v8::Local<v8::ObjectTemplate> templ) {
  gin_helper::ObjectTemplateBuilder(isolate, templ)
      .SetMethod("show", &Notification::Show)
      .SetMethod("close", &Notification::Close)
      .SetProperty("title", &Notification::GetTitle)
      .SetProperty("subtitle", &Notification::GetSubtitle)
      .SetProperty("body", &Notification::GetBody)
      .SetProperty("replyPlaceholder", &Notification::GetReplyPlaceholder)
      .SetProperty("sound", &Notification::GetSound)
      .SetProperty("closeButtonText", &Notification::GetCloseButtonText)
      .SetProperty("silent", &Notification::GetSilent)
      .SetProperty("hasReply", &Notification::GetHasReply)
      .SetProperty("urgency", &Notification::GetUrgency)
      .SetProperty("actions", &Notification::GetActions)
      .SetProperty("toastXml", &Notification::GetToastXml);
}

const char* Notification::GetTypeName() {
  return GetClassName();
}

}  // namespace api
}  // namespace lynxtron

namespace {

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  gin_helper::Dictionary dict(isolate, exports);
  dict.Set("Notification",
           lynxtron::api::Notification::GetConstructor(isolate, context));
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_binding_notification, Initialize)
