// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include "shell/common/platform_util.h"

#include <gio/gio.h>
#include <gtk/gtk.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "shell/common/global_thread.h"
#include "shell/common/linux_desktop.h"
#include "url/gurl.h"
namespace platform_util {
namespace {
void OpenUri(const std::string& uri, OpenCallback callback) {
  auto* pending = new OpenCallback(std::move(callback));
  g_app_info_launch_default_for_uri_async(
      uri.c_str(), nullptr, nullptr,
      +[](GObject*, GAsyncResult* result, gpointer data) {
        auto* pending = static_cast<OpenCallback*>(data);
        GError* error = nullptr;
        g_app_info_launch_default_for_uri_finish(result, &error);
        std::string message = error ? error->message : "";
        if (error) {
          g_error_free(error);
        }
        auto callback = std::move(*pending);
        delete pending;
        // GIO dispatches outside Chromium's task observer. Enter the UI task
        // queue so Node drains nextTick/microtasks after promise completion,
        // even when no window or timer is keeping the Node loop awake.
        lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), std::move(message)));
      },
      pending);
}
std::string FileUri(const base::FilePath& path) {
  auto* file = g_file_new_for_path(path.value().c_str());
  auto* uri = g_file_get_uri(file);
  std::string result(uri);
  g_free(uri);
  g_object_unref(file);
  return result;
}
}  // namespace
void ShowItemInFolder(const base::FilePath& path) {
  // FileManager1 selects the file when the desktop implements this service.
  g_dbus_proxy_new_for_bus(
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, nullptr,
      "org.freedesktop.FileManager1", "/org/freedesktop/FileManager1",
      "org.freedesktop.FileManager1", nullptr,
      +[](GObject*, GAsyncResult* result, gpointer data) {
        auto* path = static_cast<base::FilePath*>(data);
        GError* error = nullptr;
        auto* proxy = g_dbus_proxy_new_for_bus_finish(result, &error);
        if (proxy) {
          auto uri = FileUri(*path);
          const char* uris[] = {uri.c_str(), nullptr};
          g_dbus_proxy_call(
              proxy, "ShowItems", g_variant_new("(^ass)", uris, ""),
              G_DBUS_CALL_FLAGS_NONE, -1, nullptr,
              +[](GObject* object, GAsyncResult* result, gpointer user) {
                auto* path = static_cast<base::FilePath*>(user);
                GError* error = nullptr;
                auto* response = g_dbus_proxy_call_finish(G_DBUS_PROXY(object),
                                                          result, &error);
                if (error) {
                  OpenUri(FileUri(path->DirName()),
                          base::BindOnce([](const std::string&) {}));
                  g_error_free(error);
                }
                if (response) {
                  g_variant_unref(response);
                }
                delete path;
              },
              path);
          g_object_unref(proxy);
        } else {
          if (error) {
            g_error_free(error);
          }
          OpenUri(FileUri(path->DirName()),
                  base::BindOnce([](const std::string&) {}));
          delete path;
        }
      },
      new base::FilePath(path));
}
void OpenPath(const base::FilePath& path, OpenCallback callback) {
  if (!base::PathExists(path)) {
    std::move(callback).Run("Path does not exist");
    return;
  }
  OpenUri(FileUri(path), std::move(callback));
}
void OpenExternal(const GURL& url,
                  const OpenExternalOptions&,
                  OpenCallback callback) {
  if (!url.is_valid()) {
    std::move(callback).Run("Invalid URL");
    return;
  }
  OpenUri(url.spec(), std::move(callback));
}
void Beep() {
  if (lynxtron::EnsureLinuxDesktop()) {
    gdk_display_beep(gdk_display_get_default());
  }
}
namespace internal {
bool PlatformTrashItem(const base::FilePath& path, std::string* message) {
  auto* file = g_file_new_for_path(path.value().c_str());
  GError* error = nullptr;
  bool success = g_file_trash(file, nullptr, &error);
  g_object_unref(file);
  if (message) {
    *message = error ? error->message : "";
  }
  if (error) {
    g_error_free(error);
  }
  return success;
}
}  // namespace internal
}  // namespace platform_util
