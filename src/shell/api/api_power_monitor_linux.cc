// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include "shell/api/api_power_monitor.h"

#include <gio/gio.h>

#include <cstring>
#include <map>
#include <memory>
namespace lynxtron::api {
namespace {
struct Monitors {
  GDBusProxy* login = nullptr;
  GDBusProxy* power = nullptr;
};
std::map<const PowerMonitor*, std::shared_ptr<Monitors>> monitors;
struct Pending {
  std::weak_ptr<Monitors> state;
  PowerMonitor* monitor;
  bool power;
};
void Connect(PowerMonitor* monitor,
             const std::shared_ptr<Monitors>& state,
             bool power) {
  auto* pending = new Pending{state, monitor, power};
  auto flags = power
                   ? G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START
                   : GDBusProxyFlags(G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
                                     G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES);
  g_dbus_proxy_new_for_bus(
      G_BUS_TYPE_SYSTEM, flags, nullptr,
      power ? "org.freedesktop.UPower" : "org.freedesktop.login1",
      power ? "/org/freedesktop/UPower" : "/org/freedesktop/login1",
      power ? "org.freedesktop.UPower" : "org.freedesktop.login1.Manager",
      nullptr,
      +[](GObject*, GAsyncResult* result, gpointer data) {
        std::unique_ptr<Pending> pending(static_cast<Pending*>(data));
        GError* error = nullptr;
        auto* proxy = g_dbus_proxy_new_for_bus_finish(result, &error);
        if (error) {
          g_error_free(error);
        }
        if (!proxy) {
          return;
        }
        auto state = pending->state.lock();
        if (!state) {
          g_object_unref(proxy);
          return;
        }
        if (pending->power) {
          state->power = proxy;
          g_signal_connect(
              proxy, "g-properties-changed",
              G_CALLBACK(+[](GDBusProxy*, GVariant* changed,
                             const gchar* const*, gpointer data) {
                gboolean battery;
                if (g_variant_lookup(changed, "OnBattery", "b", &battery)) {
                  static_cast<PowerMonitor*>(data)->Emit(battery ? "on-battery"
                                                                 : "on-ac");
                }
              }),
              pending->monitor);
        } else {
          state->login = proxy;
          g_signal_connect(
              proxy, "g-signal",
              G_CALLBACK(+[](GDBusProxy*, gchar*, gchar* name, GVariant* args,
                             gpointer data) {
                auto* monitor = static_cast<PowerMonitor*>(data);
                gboolean active;
                if (std::strcmp(name, "PrepareForSleep") == 0) {
                  g_variant_get(args, "(b)", &active);
                  monitor->Emit(active ? "suspend" : "resume");
                } else if (std::strcmp(name, "PrepareForShutdown") == 0) {
                  g_variant_get(args, "(b)", &active);
                  if (active) {
                    monitor->Emit("shutdown");
                  }
                }
              }),
              pending->monitor);
        }
      },
      pending);
}
}  // namespace
bool LinuxOnBatteryPower(const PowerMonitor* monitor) {
  auto it = monitors.find(monitor);
  if (it == monitors.end() || !it->second->power) {
    return false;
  }
  auto* value =
      g_dbus_proxy_get_cached_property(it->second->power, "OnBattery");
  if (!value) {
    return false;
  }
  bool battery = g_variant_get_boolean(value);
  g_variant_unref(value);
  return battery;
}
void PowerMonitor::InitPlatformSpecificMonitors() {
  auto state = std::make_shared<Monitors>();
  monitors[this] = state;
  Connect(this, state, false);
  Connect(this, state, true);
}
void PowerMonitor::ShutdownPlatformSpecificMonitors() {
  auto it = monitors.find(this);
  if (it == monitors.end()) {
    return;
  }
  for (auto* proxy : {it->second->login, it->second->power}) {
    if (proxy) {
      g_signal_handlers_disconnect_by_data(proxy, this);
      g_object_unref(proxy);
    }
  }
  monitors.erase(it);
}
}  // namespace lynxtron::api
