// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include <gdk/x11/gdkx.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "lynx/platform/linux/gtk/lynx_gtk_view_capi.h"
#include "shell/app/native_window.h"
#include "shell/app/window_list.h"
#include "shell/common/linux_desktop.h"
#include "shell/ui/lynxtron_menu_model.h"

namespace lynxtron {
namespace {
base::WeakPtr<LynxtronMenuModel> application_menu;
struct MenuAction {
  base::WeakPtr<LynxtronMenuModel> model;
  size_t index;
  int command;
};
void Activate(const MenuAction* action, bool shortcut = false) {
  const auto current = *action;
  if (!current.model || current.index >= current.model->GetItemCount() ||
      current.model->GetCommandIdAt(current.index) != current.command ||
      !current.model->IsEnabledAt(current.index) ||
      (!current.model->IsVisibleAt(current.index) &&
       !(shortcut && current.model->WorksWhenHiddenAt(current.index)))) {
    return;
  }
  current.model->ActivatedAt(current.index, 0);
}
GMenu* BuildMenu(LynxtronMenuModel* model,
                 GSimpleActionGroup* actions,
                 GtkShortcutController* shortcuts,
                 size_t& serial) {
  auto* menu = g_menu_new();
  auto* section = g_menu_new();
  for (size_t index = 0; model && index < model->GetItemCount(); ++index) {
    const bool visible = model->IsVisibleAt(index);
    auto type = model->GetTypeAt(index);
    if (type == LynxtronMenuModel::TYPE_SEPARATOR) {
      if (!visible) {
        continue;
      }
      if (g_menu_model_get_n_items(G_MENU_MODEL(section))) {
        g_menu_append_section(menu, nullptr, G_MENU_MODEL(section));
      }
      g_object_unref(section);
      section = g_menu_new();
      continue;
    }
    auto label = base::UTF16ToUTF8(model->GetLabelAt(index));
    if (type == LynxtronMenuModel::TYPE_SUBMENU) {
      auto* submenu = BuildMenu(model->GetSubmenuModelAt(index), actions,
                                shortcuts, serial);
      if (visible) {
        g_menu_append_submenu(section, label.c_str(), G_MENU_MODEL(submenu));
      }
      g_object_unref(submenu);
      continue;
    }
    std::string name = "item" + std::to_string(serial++);
    std::string detailed = "lynxmenu." + name;
    bool radio = type == LynxtronMenuModel::TYPE_RADIO;
    bool checked_type = type == LynxtronMenuModel::TYPE_CHECK || radio;
    auto* action =
        checked_type
            ? g_simple_action_new_stateful(
                  name.c_str(), radio ? G_VARIANT_TYPE_STRING : nullptr,
                  radio ? g_variant_new_string(
                              model->IsItemCheckedAt(index) ? "selected" : "")
                        : g_variant_new_boolean(model->IsItemCheckedAt(index)))
            : g_simple_action_new(name.c_str(), nullptr);
    g_simple_action_set_enabled(action, model->IsEnabledAt(index));
    auto* data = new MenuAction{model->GetWeakPtr(), index,
                                model->GetCommandIdAt(index)};
    g_signal_connect_data(
        action, "activate",
        G_CALLBACK(+[](GSimpleAction* action, GVariant*, gpointer user) {
          const auto data = *static_cast<MenuAction*>(user);
          g_object_ref(action);
          Activate(&data);
          if (data.model && data.index < data.model->GetItemCount() &&
              data.model->GetCommandIdAt(data.index) == data.command &&
              g_action_get_state_type(G_ACTION(action))) {
            g_simple_action_set_state(
                action,
                g_variant_type_equal(g_action_get_state_type(G_ACTION(action)),
                                     G_VARIANT_TYPE_STRING)
                    ? g_variant_new_string(
                          data.model->IsItemCheckedAt(data.index) ? "selected"
                                                                  : "")
                    : g_variant_new_boolean(
                          data.model->IsItemCheckedAt(data.index)));
          }
          g_object_unref(action);
        }),
        data,
        +[](gpointer user, GClosure*) {
          delete static_cast<MenuAction*>(user);
        },
        GConnectFlags(0));
    g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));
    g_object_unref(action);
    if (visible) {
      if (radio) {
        auto* item = g_menu_item_new(label.c_str(), nullptr);
        g_menu_item_set_action_and_target(item, detailed.c_str(), "s",
                                          "selected");
        g_menu_append_item(section, item);
        g_object_unref(item);
      } else {
        g_menu_append(section, label.c_str(), detailed.c_str());
      }
    }
    ui::Accelerator accelerator;
    if (model->ShouldRegisterAcceleratorAt(index) &&
        model->GetAcceleratorAtWithParams(index, true, &accelerator)) {
      std::string key_name = accelerator.key();
      if (key_name == "Space") {
        key_name = "space";
      } else if (key_name == "Enter") {
        key_name = "Return";
      } else if (key_name == "Backspace") {
        key_name = "BackSpace";
      } else if (key_name == "Plus") {
        key_name = "plus";
      }
      guint key = gdk_keyval_from_name(key_name.c_str());
      GdkModifierType modifiers =
          GdkModifierType((accelerator.IsCtrlDown() ? GDK_CONTROL_MASK : 0) |
                          (accelerator.IsShiftDown() ? GDK_SHIFT_MASK : 0) |
                          (accelerator.IsAltDown() ? GDK_ALT_MASK : 0) |
                          (accelerator.IsCmdDown() ? GDK_SUPER_MASK : 0));
      if (key) {
        auto* retained = new MenuAction{model->GetWeakPtr(), index,
                                        model->GetCommandIdAt(index)};
        auto* callback = gtk_callback_action_new(
            +[](GtkWidget*, GVariant*, gpointer user) -> gboolean {
              Activate(static_cast<MenuAction*>(user), true);
              return TRUE;
            },
            retained,
            +[](gpointer user) { delete static_cast<MenuAction*>(user); });
        gtk_shortcut_controller_add_shortcut(
            shortcuts,
            gtk_shortcut_new(gtk_keyval_trigger_new(key, modifiers), callback));
      }
    }
  }
  if (g_menu_model_get_n_items(G_MENU_MODEL(section))) {
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(section));
  }
  g_object_unref(section);
  return menu;
}
struct MenuBinding {
  GtkWidget* widget;
  base::WeakPtr<LynxtronMenuModel> model;
  GtkShortcutController* shortcuts = nullptr;
  bool menubar;
  guint refresh = 0;
};
std::vector<MenuBinding*> menu_bindings;
void RefreshMenu(MenuBinding* binding) {
  auto* actions = g_simple_action_group_new();
  auto* shortcuts = GTK_SHORTCUT_CONTROLLER(gtk_shortcut_controller_new());
  gtk_shortcut_controller_set_scope(shortcuts, GTK_SHORTCUT_SCOPE_GLOBAL);
  size_t serial = 0;
  auto* menu = BuildMenu(binding->model.get(), actions, shortcuts, serial);
  gtk_widget_insert_action_group(binding->widget, "lynxmenu",
                                 G_ACTION_GROUP(actions));
  if (binding->shortcuts) {
    gtk_widget_remove_controller(binding->widget,
                                 GTK_EVENT_CONTROLLER(binding->shortcuts));
  }
  binding->shortcuts = shortcuts;
  gtk_widget_add_controller(binding->widget, GTK_EVENT_CONTROLLER(shortcuts));
  if (binding->menubar) {
    gtk_popover_menu_bar_set_menu_model(GTK_POPOVER_MENU_BAR(binding->widget),
                                        G_MENU_MODEL(menu));
  } else {
    gtk_popover_menu_set_menu_model(GTK_POPOVER_MENU(binding->widget),
                                    G_MENU_MODEL(menu));
  }
  g_object_unref(actions);
  g_object_unref(menu);
}
}  // namespace
void InitializeLinuxScreen();
void InitializeLinuxDesktop() {
  gtk_init_check();
  CHECK(lynx_gtk_initialize());
  InitializeLinuxScreen();
}
bool EnsureLinuxDesktop() {
  if (!gtk_init_check()) {
    return false;
  }
  InitializeLinuxScreen();
  return true;
}
void* GetLinuxContentBox(NativeWindow* window) {
  return window && !window->IsClosed() && !window->IsWindowless()
             ? g_object_get_data(G_OBJECT(window->GetNativeWindowHandle()),
                                 "lynxtron-content")
             : nullptr;
}
uintptr_t GetLinuxNativeWindowId(NativeWindow* window) {
  if (!GetLinuxContentBox(window)) {
    return 0;
  }
  auto* widget = GTK_WIDGET(window->GetNativeWindowHandle());
  gtk_widget_realize(widget);
  auto* surface = gtk_native_get_surface(GTK_NATIVE(widget));
  return surface && GDK_IS_X11_SURFACE(surface)
             ? gdk_x11_surface_get_xid(surface)
             : 0;
}
void* CreateLinuxMenu(LynxtronMenuModel* model, bool menubar) {
  auto* empty = g_menu_new();
  auto* widget = menubar
                     ? gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(empty))
                     : gtk_popover_menu_new_from_model(G_MENU_MODEL(empty));
  g_object_unref(empty);
  auto* binding =
      new MenuBinding{widget, model->GetWeakPtr(), nullptr, menubar};
  menu_bindings.push_back(binding);
  g_object_set_data_full(
      G_OBJECT(widget), "lynxtron-menu-binding", binding, +[](gpointer data) {
        auto* binding = static_cast<MenuBinding*>(data);
        if (binding->refresh) {
          g_source_remove(binding->refresh);
        }
        menu_bindings.erase(
            std::remove(menu_bindings.begin(), menu_bindings.end(), binding),
            menu_bindings.end());
        delete binding;
      });
  RefreshMenu(binding);
  return widget;
}
void RefreshLinuxMenus() {
  for (auto* binding : menu_bindings) {
    if (binding->refresh) {
      continue;
    }
    binding->refresh = g_idle_add_full(
        G_PRIORITY_DEFAULT_IDLE,
        +[](gpointer data) -> gboolean {
          auto* binding = static_cast<MenuBinding*>(data);
          binding->refresh = 0;
          // Menu property getters enter JS. Keep the widget's binding alive
          // even if a getter removes its application menu during rebuilding.
          auto* widget = GTK_WIDGET(g_object_ref(binding->widget));
          RefreshMenu(binding);
          g_object_unref(widget);
          return G_SOURCE_REMOVE;
        },
        binding, nullptr);
  }
}
void SetLinuxWindowMenu(NativeWindow* window, LynxtronMenuModel* model) {
  auto* content = GTK_BOX(GetLinuxContentBox(window));
  if (!content) {
    return;
  }
  auto* widget = GTK_WIDGET(window->GetNativeWindowHandle());
  if (auto* old =
          GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "lynxtron-menu"))) {
    gtk_box_remove(content, old);
  }
  g_object_set_data(G_OBJECT(widget), "lynxtron-menu", nullptr);
  if (!model || !model->GetItemCount()) {
    return;
  }
  auto* menu = GTK_WIDGET(CreateLinuxMenu(model, true));
  gtk_box_prepend(content, menu);
  g_object_set_data(G_OBJECT(widget), "lynxtron-menu", menu);
}
void ApplyLinuxApplicationMenu(NativeWindow* window) {
  SetLinuxWindowMenu(window, application_menu.get());
}
void SetLinuxApplicationMenu(LynxtronMenuModel* model) {
  application_menu =
      model ? model->GetWeakPtr() : base::WeakPtr<LynxtronMenuModel>();
  for (auto* window : WindowList::GetWindows()) {
    SetLinuxWindowMenu(window, model);
  }
}
}  // namespace lynxtron
