// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include <gio/gio.h>
#include <libdbusmenu-glib/menuitem.h>
#include <libdbusmenu-glib/server.h>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "shell/common/linux_desktop.h"
#include "shell/ui/lynxtron_menu_model.h"
#include "shell/ui/tray_icon.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace lynxtron {
namespace {
constexpr char kInterface[] = "org.kde.StatusNotifierItem";
constexpr char kXML[] =
    R"XML(<node><interface name="org.kde.StatusNotifierItem">
<property name="Category" type="s" access="read"/><property name="Id" type="s" access="read"/>
<property name="Title" type="s" access="read"/><property name="Status" type="s" access="read"/>
<property name="WindowId" type="i" access="read"/><property name="Menu" type="o" access="read"/>
<property name="ItemIsMenu" type="b" access="read"/><property name="IconThemePath" type="s" access="read"/>
<property name="IconName" type="s" access="read"/><property name="IconPixmap" type="a(iiay)" access="read"/>
<property name="OverlayIconName" type="s" access="read"/><property name="OverlayIconPixmap" type="a(iiay)" access="read"/>
<property name="AttentionIconName" type="s" access="read"/><property name="AttentionIconPixmap" type="a(iiay)" access="read"/>
<property name="AttentionMovieName" type="s" access="read"/><property name="ToolTip" type="(sa(iiay)ss)" access="read"/>
<method name="Activate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
<method name="SecondaryActivate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
<method name="ContextMenu"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
<method name="Scroll"><arg type="i" direction="in"/><arg type="s" direction="in"/></method>
<signal name="NewTitle"/><signal name="NewIcon"/><signal name="NewMenu"/><signal name="NewToolTip"/>
</interface></node>)XML";
class TrayIconLinux : public TrayIcon {
 public:
  TrayIconLinux(TrayIconObserver* observer, std::string guid)
      : observer_(observer), guid_(std::move(guid)) {
    static uint64_t serial = 0;
    path_ = "/StatusNotifierItem/Item" + std::to_string(++serial);
    menu_path_ = path_ + "/Menu";
    GError* error = nullptr;
    connection_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!connection_) {
      g_warning("StatusNotifierItem requires a session bus: %s",
                error ? error->message : "unavailable");
      g_clear_error(&error);
      return;
    }
    info_ = g_dbus_node_info_new_for_xml(kXML, nullptr);
    static const GDBusInterfaceVTable table = {
        +[](GDBusConnection*, const char*, const char*, const char*,
            const char* method, GVariant* args,
            GDBusMethodInvocation* invocation, gpointer data) {
          auto* self = static_cast<TrayIconLinux*>(data);
          std::string action(method);
          if (action == "Scroll") {
            g_dbus_method_invocation_return_dbus_error(
                invocation, "org.freedesktop.DBus.Error.NotSupported",
                "Tray scroll events are not exposed by this API");
            return;
          }
          int x, y;
          g_variant_get(args, "(ii)", &x, &y);
          auto* observer = self->observer_;
          auto bounds = self->GetBounds();
          g_dbus_method_invocation_return_value(invocation, nullptr);
          // JS observers may destroy the tray; do not access self afterwards.
          if (action == "Activate") {
            observer->OnClick(bounds, {x, y});
          } else if (action == "SecondaryActivate") {
            observer->OnMiddleClick(bounds);
          } else if (action == "ContextMenu") {
            observer->OnRightClick(bounds);
          }
        },
        +[](GDBusConnection*, const char*, const char*, const char*,
            const char* name, GError**, gpointer data) -> GVariant* {
          return static_cast<TrayIconLinux*>(data)->Property(name);
        },
        nullptr,
        {nullptr}};
    registration_ = g_dbus_connection_register_object(
        connection_, path_.c_str(), info_->interfaces[0], &table, this, nullptr,
        &error);
    if (!registration_) {
      g_warning("StatusNotifierItem registration failed: %s",
                error ? error->message : "unknown error");
      g_clear_error(&error);
      return;
    }
    server_ = dbusmenu_server_new(menu_path_.c_str());
    RebuildMenu();
    watcher_ = g_bus_watch_name_on_connection(
        connection_, "org.kde.StatusNotifierWatcher",
        G_BUS_NAME_WATCHER_FLAGS_NONE,
        +[](GDBusConnection* connection, const char*, const char*,
            gpointer data) {
          auto* self = static_cast<TrayIconLinux*>(data);
          g_dbus_connection_call(
              connection, "org.kde.StatusNotifierWatcher",
              "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher",
              "RegisterStatusNotifierItem",
              g_variant_new("(s)", self->path_.c_str()), nullptr,
              G_DBUS_CALL_FLAGS_NONE, 5000, nullptr,
              +[](GObject* connection, GAsyncResult* result, gpointer) {
                GError* error = nullptr;
                auto* reply = g_dbus_connection_call_finish(
                    G_DBUS_CONNECTION(connection), result, &error);
                if (reply) {
                  g_variant_unref(reply);
                }
                if (error) {
                  g_warning("Tray host registration: %s", error->message);
                  g_error_free(error);
                }
              },
              nullptr);
        },
        nullptr, this, nullptr);
  }
  ~TrayIconLinux() override {
    weak_factory_.InvalidateWeakPtrs();
    if (watcher_) {
      g_bus_unwatch_name(watcher_);
    }
    if (registration_) {
      g_dbus_connection_unregister_object(connection_, registration_);
    }
    if (server_) {
      g_object_unref(server_);
    }
    if (info_) {
      g_dbus_node_info_unref(info_);
    }
    if (connection_) {
      g_object_unref(connection_);
    }
  }
  bool valid() const { return registration_ != 0; }
  void SetImage(const gfx::Image& image) override {
    pixels_.clear();
    width_ = height_ = 0;
    if (!image.IsEmpty()) {
      auto* bitmap = image.ToSkBitmap();
      width_ = bitmap->width();
      height_ = bitmap->height();
      pixels_.resize(size_t(width_) * height_ * 4);
      for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
          auto color = bitmap->getColor(x, y);
          auto* p = pixels_.data() + (size_t(y) * width_ + x) * 4;
          // StatusNotifierItem pixmaps are ARGB bytes in network byte order.
          p[0] = SkColorGetA(color);
          p[1] = SkColorGetR(color);
          p[2] = SkColorGetG(color);
          p[3] = SkColorGetB(color);
        }
      }
    }
    Signal("NewIcon");
  }
  void SetToolTip(const std::string& tip) override {
    tip_ = tip;
    Signal("NewToolTip");
  }
  void SetTitle(const std::string& title, const std::string&) override {
    title_ = title;
    Signal("NewTitle");
  }
  std::string GetTitle() const override { return title_; }
  void SetContextMenu(LynxtronMenuModel* menu) override {
    menu_ = menu ? menu->GetWeakPtr() : base::WeakPtr<LynxtronMenuModel>();
    RebuildMenu();
    Signal("NewMenu");
  }
  void PopUpContextMenu(LynxtronMenuModel* menu, const TrayPoint&) override {
    SetContextMenu(menu);
    // The panel owns the popup surface on Wayland. Its exported menu is usable
    // from the panel, but this protocol cannot force every host to open it.
    g_warning(
        "Programmatic tray popup is controlled by the StatusNotifier host");
  }
  void CloseContextMenu() override {
    g_warning(
        "Programmatic tray popup dismissal is controlled by the StatusNotifier "
        "host");
  }
  TrayBounds GetBounds() const override {
    return {};
  }  // No portable panel geometry.
  std::string GetGUID() const override { return guid_; }

 private:
  struct Action {
    base::WeakPtr<LynxtronMenuModel> model;
    size_t index;
    int command;
  };
  DbusmenuMenuitem* BuildMenu(LynxtronMenuModel* model) {
    auto* root = dbusmenu_menuitem_new();
    for (size_t i = 0; model && i < model->GetItemCount(); ++i) {
      auto type = model->GetTypeAt(i);
      auto* item = type == LynxtronMenuModel::TYPE_SUBMENU
                       ? BuildMenu(model->GetSubmenuModelAt(i))
                       : dbusmenu_menuitem_new();
      dbusmenu_menuitem_property_set(
          item, "label", base::UTF16ToUTF8(model->GetLabelAt(i)).c_str());
      dbusmenu_menuitem_property_set_bool(item, "enabled",
                                          model->IsEnabledAt(i));
      dbusmenu_menuitem_property_set_bool(item, "visible",
                                          model->IsVisibleAt(i));
      if (type == LynxtronMenuModel::TYPE_SEPARATOR) {
        dbusmenu_menuitem_property_set(item, "type", "separator");
      }
      if (type == LynxtronMenuModel::TYPE_SUBMENU) {
        dbusmenu_menuitem_property_set(item, "children-display", "submenu");
      }
      if (type == LynxtronMenuModel::TYPE_CHECK ||
          type == LynxtronMenuModel::TYPE_RADIO) {
        dbusmenu_menuitem_property_set(
            item, "toggle-type",
            type == LynxtronMenuModel::TYPE_CHECK ? "checkmark" : "radio");
        dbusmenu_menuitem_property_set_int(item, "toggle-state",
                                           model->IsItemCheckedAt(i) ? 1 : 0);
      }
      if (type != LynxtronMenuModel::TYPE_SUBMENU &&
          type != LynxtronMenuModel::TYPE_SEPARATOR) {
        auto* action =
            new Action{model->GetWeakPtr(), i, model->GetCommandIdAt(i)};
        g_signal_connect_data(
            item, "item-activated",
            G_CALLBACK(+[](DbusmenuMenuitem*, guint, gpointer data) {
              auto action = *static_cast<Action*>(data);
              if (action.model && action.index < action.model->GetItemCount() &&
                  action.model->GetCommandIdAt(action.index) ==
                      action.command &&
                  action.model->IsEnabledAt(action.index) &&
                  action.model->IsVisibleAt(action.index)) {
                action.model->ActivatedAt(action.index, 0);
              }
            }),
            action,
            +[](gpointer data, GClosure*) {
              delete static_cast<Action*>(data);
            },
            GConnectFlags(0));
      }
      dbusmenu_menuitem_child_append(root, item);
      g_object_unref(item);
    }
    return root;
  }
  void RebuildMenu() {
    if (!server_) {
      return;
    }
    auto* root = BuildMenu(menu_.get());
    auto* weak = new base::WeakPtr<TrayIconLinux>(weak_factory_.GetWeakPtr());
    g_signal_connect_data(
        root, "about-to-show",
        G_CALLBACK(+[](DbusmenuMenuitem*, gpointer data) -> gboolean {
          auto self = *static_cast<base::WeakPtr<TrayIconLinux>*>(data);
          if (!self || !self->menu_) {
            return FALSE;
          }
          auto model = self->menu_;
          model->MenuWillShow();
          if (self) {
            self->RebuildMenu();
          }
          return TRUE;
        }),
        weak,
        +[](gpointer data, GClosure*) {
          delete static_cast<base::WeakPtr<TrayIconLinux>*>(data);
        },
        GConnectFlags(0));
    dbusmenu_server_set_root(server_, root);
    g_object_unref(root);
  }
  GVariant* Pixmap(bool icon) {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(iiay)"));
    if (icon && !pixels_.empty()) {
      g_variant_builder_add(
          &builder, "(ii@ay)", width_, height_,
          g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, pixels_.data(),
                                    pixels_.size(), 1));
    }
    return g_variant_builder_end(&builder);
  }
  GVariant* Property(const char* property) {
    std::string name(property);
    if (name == "Category") {
      return g_variant_new_string("ApplicationStatus");
    }
    if (name == "Id") {
      return g_variant_new_string(guid_.c_str());
    }
    if (name == "Title") {
      return g_variant_new_string(title_.c_str());
    }
    if (name == "Status") {
      return g_variant_new_string("Active");
    }
    if (name == "WindowId") {
      return g_variant_new_int32(0);
    }
    if (name == "Menu") {
      return g_variant_new_object_path(menu_ ? menu_path_.c_str() : "/");
    }
    if (name == "ItemIsMenu") {
      return g_variant_new_boolean(FALSE);
    }
    if (name == "IconPixmap" || name == "OverlayIconPixmap" ||
        name == "AttentionIconPixmap") {
      return Pixmap(name == "IconPixmap");
    }
    if (name == "ToolTip") {
      return g_variant_new("(s@a(iiay)ss)", "", Pixmap(true), title_.c_str(),
                           tip_.c_str());
    }
    return g_variant_new_string("");
  }
  void Signal(const char* name) {
    if (registration_) {
      g_dbus_connection_emit_signal(connection_, nullptr, path_.c_str(),
                                    kInterface, name, nullptr, nullptr);
    }
  }
  TrayIconObserver* observer_;
  std::string guid_, title_, tip_, path_, menu_path_;
  GDBusConnection* connection_ = nullptr;
  GDBusNodeInfo* info_ = nullptr;
  DbusmenuServer* server_ = nullptr;
  guint registration_ = 0, watcher_ = 0;
  int width_ = 0, height_ = 0;
  std::vector<guchar> pixels_;
  base::WeakPtr<LynxtronMenuModel> menu_;
  base::WeakPtrFactory<TrayIconLinux> weak_factory_{this};
};
}  // namespace
std::unique_ptr<TrayIcon> CreateLinuxTrayIcon(TrayIconObserver* observer,
                                              const std::string& guid) {
  if (!EnsureLinuxDesktop()) {
    return nullptr;
  }
  auto tray = std::make_unique<TrayIconLinux>(observer, guid);
  return tray->valid() ? std::move(tray) : nullptr;
}
}  // namespace lynxtron
