// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include "shell/api/api_menu.h"

#include <gtk/gtk.h>

#include "shell/api/api_base_window.h"
#include "shell/common/gin_helper/function_template.h"
#include "shell/common/linux_desktop.h"
namespace lynxtron::api {
namespace {
class MenuLinux : public Menu {
 public:
  explicit MenuLinux(gin::Arguments* args) : Menu(args) {}
  ~MenuLinux() override {
    if (popup_) {
      g_signal_handlers_disconnect_by_data(popup_, this);
      gtk_widget_destroy(popup_);
    }
  }

 protected:
  void PopupAt(BaseWindow* window,
               int x,
               int y,
               int,
               base::OnceClosure callback) override {
    if (!EnsureLinuxDesktop() || !window || window->window()->IsWindowless()) {
      std::move(callback).Run();
      return;
    }
    if (popup_) {
      ClosePopupAt(0);
    }
    model()->MenuWillShow();
    popup_ = GTK_WIDGET(CreateLinuxMenu(model()));
    callback_ = BindSelfToClosure(std::move(callback));
    g_signal_connect(popup_, "selection-done",
                     G_CALLBACK(+[](GtkMenuShell* popup, gpointer user) {
                       auto* self = static_cast<MenuLinux*>(user);
                       self->popup_ = nullptr;
                       g_signal_handlers_disconnect_by_data(popup, self);
                       gtk_widget_destroy(GTK_WIDGET(popup));
                       auto callback = std::move(self->callback_);
                       self->model()->MenuWillClose();
                       if (callback) {
                         std::move(callback).Run();
                       }
                     }),
                     this);
    auto* native = gtk_widget_get_window(
        GTK_WIDGET(window->window()->GetNativeWindowHandle()));
    if (x < 0 || y < 0) {
      auto* seat = gdk_display_get_default_seat(gdk_window_get_display(native));
      gdk_device_get_position(gdk_seat_get_pointer(seat), nullptr, &x, &y);
      int ox, oy;
      gdk_window_get_origin(native, &ox, &oy);
      x -= ox;
      y -= oy;
    }
    GdkRectangle rect{x, y, 1, 1};
    gtk_menu_popup_at_rect(GTK_MENU(popup_), native, &rect,
                           GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST,
                           nullptr);
  }
  void ClosePopupAt(int32_t) override {
    if (!popup_) {
      return;
    }
    auto* popup = popup_;
    popup_ = nullptr;
    g_signal_handlers_disconnect_by_data(popup, this);
    gtk_menu_popdown(GTK_MENU(popup));
    gtk_widget_destroy(popup);
    auto callback = std::move(callback_);
    model()->MenuWillClose();
    if (callback) {
      std::move(callback).Run();
    }
  }

 private:
  GtkWidget* popup_ = nullptr;
  base::OnceClosure callback_;
};
}  // namespace
Menu* Menu::New(gin::Arguments* args) {
  auto* menu = new MenuLinux(args);
  gin_helper::CallMethod(args->isolate(), menu, "_init");
  return menu;
}
void Menu::SetApplicationMenu(Menu* menu) {
  SetLinuxApplicationMenu(menu ? menu->model() : nullptr);
}
}  // namespace lynxtron::api
