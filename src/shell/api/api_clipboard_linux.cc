// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include "shell/api/api_clipboard.h"

#include <gtk/gtk.h>

#include "shell/common/linux_desktop.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
namespace lynxtron::api::clipboard {
namespace {
GtkClipboard* Get() {
  return EnsureLinuxDesktop() ? gtk_clipboard_get(GDK_SELECTION_CLIPBOARD)
                              : nullptr;
}
void Contents(GtkClipboard*,
              GtkSelectionData* selection,
              guint info,
              gpointer user) {
  const auto& data = *static_cast<ClipboardData*>(user);
  if (info == 0 && data.text) {
    gtk_selection_data_set_text(selection, data.text->c_str(),
                                data.text->size());
  }
  if (info == 1 && data.html) {
    gtk_selection_data_set(
        selection, gdk_atom_intern_static_string("text/html"), 8,
        reinterpret_cast<const guchar*>(data.html->data()), data.html->size());
  }
  if (info == 2 && data.image && !data.image->IsEmpty()) {
    const SkBitmap* bitmap = data.image->ToSkBitmap();
    std::vector<guchar> rgba(bitmap->width() * bitmap->height() * 4);
    for (int y = 0; y < bitmap->height(); ++y) {
      for (int x = 0; x < bitmap->width(); ++x) {
        auto c = bitmap->getColor(x, y);
        auto* pixel = &rgba[(y * bitmap->width() + x) * 4];
        pixel[0] = SkColorGetR(c);
        pixel[1] = SkColorGetG(c);
        pixel[2] = SkColorGetB(c);
        pixel[3] = SkColorGetA(c);
      }
    }
    auto* pixbuf = gdk_pixbuf_new_from_data(
        rgba.data(), GDK_COLORSPACE_RGB, TRUE, 8, bitmap->width(),
        bitmap->height(), bitmap->width() * 4, nullptr, nullptr);
    gtk_selection_data_set_pixbuf(selection, pixbuf);
    g_object_unref(pixbuf);
  }
}
}  // namespace
std::vector<std::string> AvailableFormats() {
  std::vector<std::string> formats;
  auto* clipboard = Get();
  if (!clipboard) {
    return formats;
  }
  if (gtk_clipboard_wait_is_text_available(clipboard)) {
    formats.push_back("text/plain");
  }
  if (gtk_clipboard_wait_is_target_available(
          clipboard, gdk_atom_intern_static_string("text/html"))) {
    formats.push_back("text/html");
  }
  if (gtk_clipboard_wait_is_image_available(clipboard)) {
    formats.push_back("image/png");
  }
  return formats;
}
void Clear() {
  if (auto* clipboard = Get()) {
    gtk_clipboard_clear(clipboard);
  }
}
std::string ReadText() {
  auto* clipboard = Get();
  if (!clipboard) {
    return {};
  }
  auto* text = gtk_clipboard_wait_for_text(clipboard);
  std::string result = text ? text : "";
  g_free(text);
  return result;
}
std::string ReadHTML() {
  auto* clipboard = Get();
  if (!clipboard) {
    return {};
  }
  auto* data = gtk_clipboard_wait_for_contents(
      clipboard, gdk_atom_intern_static_string("text/html"));
  std::string result;
  if (data && gtk_selection_data_get_length(data) > 0) {
    result.assign(
        reinterpret_cast<const char*>(gtk_selection_data_get_data(data)),
        gtk_selection_data_get_length(data));
  }
  if (data) {
    gtk_selection_data_free(data);
  }
  return result;
}
gfx::Image ReadImage() {
  auto* clipboard = Get();
  if (!clipboard) {
    return {};
  }
  auto* pixbuf = gtk_clipboard_wait_for_image(clipboard);
  if (!pixbuf) {
    return {};
  }
  SkBitmap bitmap;
  bitmap.allocN32Pixels(gdk_pixbuf_get_width(pixbuf),
                        gdk_pixbuf_get_height(pixbuf));
  const int channels = gdk_pixbuf_get_n_channels(pixbuf),
            stride = gdk_pixbuf_get_rowstride(pixbuf);
  auto* pixels = gdk_pixbuf_get_pixels(pixbuf);
  for (int y = 0; y < bitmap.height(); ++y) {
    for (int x = 0; x < bitmap.width(); ++x) {
      auto* pixel = pixels + y * stride + x * channels;
      *bitmap.getAddr32(x, y) = SkPreMultiplyARGB(
          channels == 4 ? pixel[3] : 255, pixel[0], pixel[1], pixel[2]);
    }
  }
  g_object_unref(pixbuf);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}
void Write(const ClipboardData& data) {
  auto* clipboard = Get();
  if (!clipboard) {
    return;
  }
  auto* targets = gtk_target_list_new(nullptr, 0);
  if (data.text) {
    gtk_target_list_add_text_targets(targets, 0);
  }
  if (data.html) {
    gtk_target_list_add(targets, gdk_atom_intern_static_string("text/html"), 0,
                        1);
  }
  if (data.image && !data.image->IsEmpty()) {
    gtk_target_list_add_image_targets(targets, 2, TRUE);
  }
  gint count;
  auto* entries = gtk_target_table_new_from_list(targets, &count);
  if (!count) {
    gtk_target_table_free(entries, count);
    gtk_target_list_unref(targets);
    gtk_clipboard_clear(clipboard);
    return;
  }
  auto* retained = new ClipboardData(data);
  if (!gtk_clipboard_set_with_data(
          clipboard, entries, count, Contents,
          +[](GtkClipboard*, gpointer user) {
            delete static_cast<ClipboardData*>(user);
          },
          retained)) {
    delete retained;
  }
  gtk_target_table_free(entries, count);
  gtk_target_list_unref(targets);
  gtk_clipboard_set_can_store(clipboard, nullptr, 0);
}
void WriteText(const std::string& text) {
  ClipboardData data;
  data.text = text;
  Write(data);
}
void WriteHTML(const std::string& html) {
  ClipboardData data;
  data.html = html;
  Write(data);
}
void WriteImage(const gfx::Image& image) {
  ClipboardData data;
  data.image = image;
  Write(data);
}
}  // namespace lynxtron::api::clipboard
