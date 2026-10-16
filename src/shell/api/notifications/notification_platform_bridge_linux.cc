// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include <gio/gio.h>

#include <cstring>

#include "shell/api/notifications/platform_notification_service.h"
#include "shell/app/application.h"
#include "third_party/skia/include/core/SkBitmap.h"
namespace lynxtron {
namespace {
class LinuxNotification : public Notification {
 public:
  LinuxNotification(NotificationDelegate* delegate,
                    NotificationOptions* options)
      : Notification(delegate, options) {
    GError* error = nullptr;
    proxy_ = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, nullptr,
        "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications", nullptr, &error);
    if (error) {
      g_error_free(error);
    }
    if (proxy_) {
      g_signal_connect(
          proxy_, "g-signal",
          G_CALLBACK(+[](GDBusProxy*, gchar*, gchar* signal, GVariant* params,
                         gpointer user) {
            auto* self = static_cast<LinuxNotification*>(user);
            if (std::strcmp(signal, "NotificationClosed") == 0) {
              guint id;
              guint reason;
              g_variant_get(params, "(uu)", &id, &reason);
              if (id != self->id_) {
                return;
              }
              self->id_ = 0;
              if (self->delegate_) {
                self->delegate_->NotificationClosed();
              }
            } else if (std::strcmp(signal, "ActionInvoked") == 0) {
              guint id;
              const char* action;
              g_variant_get(params, "(u&s)", &id, &action);
              if (id != self->id_ || !self->delegate_) {
                return;
              }
              if (std::strcmp(action, "default") == 0) {
                self->delegate_->NotificationClick();
              } else {
                gchar* end;
                auto index = g_ascii_strtoull(action, &end, 10);
                if (*end == '\0' && index < self->options_.actions.size()) {
                  self->delegate_->NotificationAction(index);
                }
              }
            }
          }),
          this);
    }
  }
  ~LinuxNotification() override {
    if (proxy_) {
      g_signal_handlers_disconnect_by_data(proxy_, this);
      Dismiss();
      g_object_unref(proxy_);
    }
  }
  void Show() override {
    if (!proxy_) {
      if (delegate_) {
        delegate_->NotificationFailed();
      }
      return;
    }
    dismissed_ = false;
    GVariantBuilder actions, hints;
    g_variant_builder_init(&actions, G_VARIANT_TYPE_STRING_ARRAY);
    g_variant_builder_init(&hints, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&actions, "s", "default");
    g_variant_builder_add(&actions, "s", "Open");
    for (size_t i = 0; i < options_.actions.size(); ++i) {
      auto key = std::to_string(i);
      g_variant_builder_add(&actions, "s", key.c_str());
      g_variant_builder_add(&actions, "s", options_.actions[i].text.c_str());
    }
    g_variant_builder_add(&hints, "{sv}", "suppress-sound",
                          g_variant_new_boolean(options_.silent));
    g_variant_builder_add(&hints, "{sv}", "urgency",
                          g_variant_new_byte(options_.urgency == "critical" ? 2
                                             : options_.urgency == "low" ? 0
                                                                         : 1));
    if (!options_.icon.IsEmpty()) {
      auto* bitmap = options_.icon.ToSkBitmap();
      if (bitmap->width() <= 512 && bitmap->height() <= 512) {
        std::vector<guchar> pixels(bitmap->width() * bitmap->height() * 4);
        for (int y = 0; y < bitmap->height(); ++y) {
          for (int x = 0; x < bitmap->width(); ++x) {
            auto c = bitmap->getColor(x, y);
            auto* p = &pixels[(y * bitmap->width() + x) * 4];
            p[0] = SkColorGetR(c);
            p[1] = SkColorGetG(c);
            p[2] = SkColorGetB(c);
            p[3] = SkColorGetA(c);
          }
        }
        auto* bytes = g_variant_new_fixed_array(
            G_VARIANT_TYPE_BYTE, pixels.data(), pixels.size(), 1);
        g_variant_builder_add(
            &hints, "{sv}", "image-data",
            g_variant_new("(iiibii@ay)", bitmap->width(), bitmap->height(),
                          bitmap->width() * 4, TRUE, 8, 4, bytes));
      }
    }
    auto* escaped = g_markup_escape_text(options_.body.c_str(), -1);
    auto* params = g_variant_new(
        "(susss@as@a{sv}i)", Application::Get()->GetName().c_str(), id_, "",
        options_.title.c_str(), escaped, g_variant_builder_end(&actions),
        g_variant_builder_end(&hints),
        options_.timeout_type == "never" ? 0 : -1);
    g_free(escaped);
    auto* weak = new base::WeakPtr<Notification>(GetWeakPtr());
    g_dbus_proxy_call(
        proxy_, "Notify", params, G_DBUS_CALL_FLAGS_NONE, -1, nullptr,
        +[](GObject* object, GAsyncResult* result, gpointer user) {
          auto weak = *static_cast<base::WeakPtr<Notification>*>(user);
          delete static_cast<base::WeakPtr<Notification>*>(user);
          GError* error = nullptr;
          auto* response =
              g_dbus_proxy_call_finish(G_DBUS_PROXY(object), result, &error);
          if (weak) {
            auto* self = static_cast<LinuxNotification*>(weak.get());
            if (response) {
              g_variant_get(response, "(u)", &self->id_);
              if (self->dismissed_) {
                self->Dismiss();
              } else if (self->delegate_) {
                self->delegate_->NotificationDisplayed();
              }
            } else if (self->delegate_) {
              self->delegate_->NotificationFailed();
            }
          } else if (response) {
            guint id;
            g_variant_get(response, "(u)", &id);
            g_dbus_proxy_call(G_DBUS_PROXY(object), "CloseNotification",
                              g_variant_new("(u)", id), G_DBUS_CALL_FLAGS_NONE,
                              -1, nullptr, nullptr, nullptr);
          }
          if (response) {
            g_variant_unref(response);
          }
          if (error) {
            g_error_free(error);
          }
        },
        weak);
  }
  void Dismiss() override {
    dismissed_ = true;
    if (proxy_ && id_) {
      g_dbus_proxy_call(proxy_, "CloseNotification", g_variant_new("(u)", id_),
                        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, nullptr);
    }
  }

 private:
  GDBusProxy* proxy_ = nullptr;
  guint id_ = 0;
  bool dismissed_ = false;
};
class LinuxNotificationService : public PlatformNotificationService {
 public:
  std::unique_ptr<Notification> CreateNotification(
      NotificationDelegate* delegate,
      NotificationOptions* options) override {
    return std::make_unique<LinuxNotification>(delegate, options);
  }
};
}  // namespace
std::unique_ptr<PlatformNotificationService> CreateLinuxNotificationService() {
  return std::make_unique<LinuxNotificationService>();
}
}  // namespace lynxtron
