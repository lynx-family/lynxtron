// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include "shell/ui/tray_icon.h"

#include <gtk/gtk.h>

#include "shell/common/linux_desktop.h"
#include "shell/ui/lynxtron_menu_model.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
namespace lynxtron {
namespace {
class TrayIconLinux : public TrayIcon {
 public:
  TrayIconLinux(TrayIconObserver* observer, const std::string& guid)
      : observer_(observer), guid_(guid) {
    icon_ = gtk_status_icon_new();
    gtk_status_icon_set_visible(icon_, TRUE);
    g_signal_connect(icon_, "activate",
                     G_CALLBACK(+[](GtkStatusIcon*, gpointer user) {
                       auto* self = static_cast<TrayIconLinux*>(user);
                       auto bounds = self->GetBounds();
                       self->observer_->OnClick(bounds, {bounds.x, bounds.y});
                     }),
                     this);
    g_signal_connect(
        icon_, "popup-menu",
        G_CALLBACK(+[](GtkStatusIcon*, guint, guint, gpointer user) {
          auto* self = static_cast<TrayIconLinux*>(user);
          if (auto* model = self->menu_.get()) {
            self->PopUpContextMenu(model, {});
          }
          self->observer_->OnRightClick(self->GetBounds());
        }),
        this);
  }
  ~TrayIconLinux() override {
    CloseContextMenu();
    g_signal_handlers_disconnect_by_data(icon_, this);
    gtk_status_icon_set_visible(icon_, FALSE);
    g_object_unref(icon_);
  }
  void SetImage(const gfx::Image& image) override {
    if (image.IsEmpty()) {
      gtk_status_icon_set_from_pixbuf(icon_, nullptr);
      return;
    }
    auto* bitmap = image.ToSkBitmap();
    auto* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, bitmap->width(),
                                  bitmap->height());
    auto* pixels = gdk_pixbuf_get_pixels(pixbuf);
    int stride = gdk_pixbuf_get_rowstride(pixbuf);
    for (int y = 0; y < bitmap->height(); ++y) {
      for (int x = 0; x < bitmap->width(); ++x) {
        auto c = bitmap->getColor(x, y);
        auto* p = pixels + y * stride + x * 4;
        p[0] = SkColorGetR(c);
        p[1] = SkColorGetG(c);
        p[2] = SkColorGetB(c);
        p[3] = SkColorGetA(c);
      }
    }
    gtk_status_icon_set_from_pixbuf(icon_, pixbuf);
    g_object_unref(pixbuf);
  }
  void SetToolTip(const std::string& tip) override {
    gtk_status_icon_set_tooltip_text(icon_, tip.c_str());
  }
  void SetTitle(const std::string& title, const std::string&) override {
    title_ = title;
    gtk_status_icon_set_title(icon_, title.c_str());
  }
  std::string GetTitle() const override { return title_; }
  void SetContextMenu(LynxtronMenuModel* model) override {
    menu_ = model ? model->GetWeakPtr() : base::WeakPtr<LynxtronMenuModel>();
  }
  void PopUpContextMenu(LynxtronMenuModel* model, const TrayPoint&) override {
    CloseContextMenu();
    if (!model) {
      return;
    }
    model->MenuWillShow();
    popup_ = GTK_WIDGET(CreateLinuxMenu(model));
    auto* weak = new base::WeakPtr<LynxtronMenuModel>(model->GetWeakPtr());
    g_signal_connect_data(
        popup_, "selection-done", G_CALLBACK(+[](GtkMenuShell*, gpointer user) {
          auto& model = *static_cast<base::WeakPtr<LynxtronMenuModel>*>(user);
          if (model) {
            model->MenuWillClose();
          }
        }),
        weak,
        +[](gpointer user, GClosure*) {
          delete static_cast<base::WeakPtr<LynxtronMenuModel>*>(user);
        },
        GConnectFlags(0));
    gtk_menu_popup(GTK_MENU(popup_), nullptr, nullptr,
                   gtk_status_icon_position_menu, icon_, 0,
                   gtk_get_current_event_time());
  }
  void CloseContextMenu() override {
    if (popup_) {
      gtk_widget_destroy(popup_);
      popup_ = nullptr;
    }
  }
  TrayBounds GetBounds() const override {
    GdkRectangle rect{};
    gtk_status_icon_get_geometry(icon_, nullptr, &rect, nullptr);
    return {rect.x, rect.y, rect.width, rect.height};
  }
  std::string GetGUID() const override { return guid_; }

 private:
  GtkStatusIcon* icon_;
  GtkWidget* popup_ = nullptr;
  TrayIconObserver* observer_;
  base::WeakPtr<LynxtronMenuModel> menu_;
  std::string guid_, title_;
};
}  // namespace
std::unique_ptr<TrayIcon> CreateLinuxTrayIcon(TrayIconObserver* observer,
                                              const std::string& guid) {
  if (!EnsureLinuxDesktop()) {
    return nullptr;
  }
  return std::make_unique<TrayIconLinux>(observer, guid);
}
}  // namespace lynxtron
