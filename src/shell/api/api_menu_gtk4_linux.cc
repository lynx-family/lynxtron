// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include <gtk/gtk.h>

#include "shell/api/api_base_window.h"
#include "shell/api/api_menu.h"
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
      gtk_widget_unparent(popup_);
      g_object_unref(popup_);
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
    callback_ = BindSelfToClosure(std::move(callback));
    model()->MenuWillShow();
    auto* parent = GTK_WIDGET(GetLinuxContentBox(window->window()));
    if (!parent) {
      auto done = std::move(callback_);
      model()->MenuWillClose();
      if (done) {
        std::move(done).Run();
      }
      return;
    }
    popup_ = GTK_WIDGET(CreateLinuxMenu(model()));
    g_object_ref_sink(popup_);
    gtk_widget_set_parent(popup_, parent);
    g_signal_connect(popup_, "closed",
                     G_CALLBACK(+[](GtkPopover*, gpointer user) {
                       static_cast<MenuLinux*>(user)->ClosePopupAt(0);
                     }),
                     this);
    if (x < 0 || y < 0) {
      x = y = 0;
      auto* display = gtk_widget_get_display(parent);
      auto* seat = gdk_display_get_default_seat(display);
      auto* pointer = seat ? gdk_seat_get_pointer(seat) : nullptr;
      double px, py;
      auto* native = gtk_widget_get_native(parent);
      if (pointer && native &&
          gdk_device_get_surface_at_position(pointer, &px, &py) ==
              gtk_native_get_surface(native)) {
        graphene_point_t source{float(px), float(py)}, local;
        if (gtk_widget_compute_point(GTK_WIDGET(native), parent, &source,
                                     &local)) {
          x = local.x;
          y = local.y;
        }
      }
    }
    GdkRectangle rect{x, y, 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popup_), &rect);
    gtk_popover_popup(GTK_POPOVER(popup_));
  }

  void ClosePopupAt(int32_t) override {
    if (!popup_) {
      return;
    }
    auto* popup = popup_;
    popup_ = nullptr;
    g_signal_handlers_disconnect_by_data(popup, this);
    gtk_popover_popdown(GTK_POPOVER(popup));
    gtk_widget_unparent(popup);
    g_object_unref(popup);
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
