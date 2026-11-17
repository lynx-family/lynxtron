// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include <gtk/gtk.h>

#include "shell/api/api_clipboard.h"
#include "shell/common/linux_desktop.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "ui/gfx/image/image_skia.h"

namespace lynxtron::api::clipboard {
namespace {
GdkClipboard* Get() {
  return EnsureLinuxDesktop()
             ? gdk_display_get_clipboard(gdk_display_get_default())
             : nullptr;
}
// The existing JS API is synchronous. Pump the owner GLib context until the
// asynchronous GDK operation finishes; cancellation also finishes through its
// callback, so no callback can retain a pointer to a returned stack frame.
struct Read {
  GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
  GCancellable* cancel = g_cancellable_new();
  GInputStream* stream = nullptr;
  GdkTexture* texture = nullptr;
  std::string text;
  bool done = false;
  guint timeout = 0;
  Read() {
    timeout = g_timeout_add_seconds(
        10,
        +[](gpointer data) -> gboolean {
          auto* read = static_cast<Read*>(data);
          read->timeout = 0;
          g_cancellable_cancel(read->cancel);
          return G_SOURCE_REMOVE;
        },
        this);
  }
  ~Read() {
    if (timeout) {
      g_source_remove(timeout);
    }
    if (stream) {
      g_object_unref(stream);
    }
    if (texture) {
      g_object_unref(texture);
    }
    g_object_unref(cancel);
    g_main_loop_unref(loop);
  }
  void Finish() {
    done = true;
    g_main_loop_quit(loop);
  }
  void Wait() {
    if (!done) {
      g_main_loop_run(loop);
    }
  }
  void Next() {
    g_input_stream_read_bytes_async(
        stream, 65536, G_PRIORITY_DEFAULT, cancel,
        +[](GObject* object, GAsyncResult* result, gpointer data) {
          auto* read = static_cast<Read*>(data);
          GError* error = nullptr;
          auto* bytes = g_input_stream_read_bytes_finish(G_INPUT_STREAM(object),
                                                         result, &error);
          gsize size = 0;
          auto* buffer =
              bytes ? static_cast<const char*>(g_bytes_get_data(bytes, &size))
                    : nullptr;
          constexpr size_t kMaxHTMLBytes = 16 * 1024 * 1024;
          bool invalid = error || size > kMaxHTMLBytes - read->text.size();
          if (!invalid && size) {
            read->text.append(buffer, size);
          }
          if (bytes) {
            g_bytes_unref(bytes);
          }
          if (error) {
            g_error_free(error);
          }
          if (invalid) {
            read->text.clear();
          }
          if (invalid || !size) {
            read->Finish();
          } else {
            read->Next();
          }
        },
        this);
  }
};
}  // namespace
std::vector<std::string> AvailableFormats() {
  std::vector<std::string> result;
  auto* clipboard = Get();
  if (!clipboard) {
    return result;
  }
  auto* formats = gdk_clipboard_get_formats(clipboard);
  if (gdk_content_formats_contain_gtype(formats, G_TYPE_STRING) ||
      gdk_content_formats_contain_mime_type(formats,
                                            "text/plain;charset=utf-8") ||
      gdk_content_formats_contain_mime_type(formats, "text/plain")) {
    result.push_back("text/plain");
  }
  if (gdk_content_formats_contain_mime_type(formats, "text/html")) {
    result.push_back("text/html");
  }
  if (gdk_content_formats_contain_gtype(formats, GDK_TYPE_TEXTURE) ||
      gdk_content_formats_contain_mime_type(formats, "image/png")) {
    result.push_back("image/png");
  }
  return result;
}
void Clear() {
  if (auto* clipboard = Get()) {
    gdk_clipboard_set_content(clipboard, nullptr);
  }
}
std::string ReadText() {
  auto* clipboard = Get();
  if (!clipboard) {
    return {};
  }
  Read read;
  gdk_clipboard_read_text_async(
      clipboard, read.cancel,
      +[](GObject* object, GAsyncResult* result, gpointer data) {
        auto* read = static_cast<Read*>(data);
        GError* error = nullptr;
        auto* text = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(object),
                                                    result, &error);
        if (text && !error) {
          read->text = text;
        }
        g_free(text);
        if (error) {
          g_error_free(error);
        }
        read->Finish();
      },
      &read);
  read.Wait();
  return read.text;
}
std::string ReadHTML() {
  auto* clipboard = Get();
  if (!clipboard) {
    return {};
  }
  Read read;
  const char* types[] = {"text/html", nullptr};
  gdk_clipboard_read_async(
      clipboard, types, G_PRIORITY_DEFAULT, read.cancel,
      +[](GObject* object, GAsyncResult* result, gpointer data) {
        auto* read = static_cast<Read*>(data);
        GError* error = nullptr;
        const char* type = nullptr;
        read->stream = gdk_clipboard_read_finish(GDK_CLIPBOARD(object), result,
                                                 &type, &error);
        if (error) {
          g_error_free(error);
        }
        if (read->stream) {
          read->Next();
        } else {
          read->Finish();
        }
      },
      &read);
  read.Wait();
  return read.text;
}
gfx::Image ReadImage() {
  auto* clipboard = Get();
  if (!clipboard) {
    return {};
  }
  Read read;
  gdk_clipboard_read_texture_async(
      clipboard, read.cancel,
      +[](GObject* object, GAsyncResult* result, gpointer data) {
        auto* read = static_cast<Read*>(data);
        GError* error = nullptr;
        read->texture = gdk_clipboard_read_texture_finish(GDK_CLIPBOARD(object),
                                                          result, &error);
        if (error) {
          g_error_free(error);
        }
        read->Finish();
      },
      &read);
  read.Wait();
  if (!read.texture) {
    return {};
  }
  int width = gdk_texture_get_width(read.texture),
      height = gdk_texture_get_height(read.texture);
  if (width <= 0 || height <= 0 ||
      size_t(width) > (64 * 1024 * 1024 / 4) / size_t(height)) {
    return {};
  }
  // GDK's explicit RGBA premultiplied download avoids native-endian ambiguity.
  std::vector<guchar> pixels(size_t(width) * height * 4);
  auto* downloader = gdk_texture_downloader_new(read.texture);
  gdk_texture_downloader_set_format(downloader,
                                    GDK_MEMORY_R8G8B8A8_PREMULTIPLIED);
  gdk_texture_downloader_download_into(downloader, pixels.data(),
                                       size_t(width) * 4);
  gdk_texture_downloader_free(downloader);
  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(width, height)) {
    return {};
  }
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      auto* p = pixels.data() + (size_t(y) * width + x) * 4;
      *bitmap.getAddr32(x, y) = SkPackARGB32(p[3], p[0], p[1], p[2]);
    }
  }
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}
void Write(const ClipboardData& data) {
  auto* clipboard = Get();
  if (!clipboard) {
    return;
  }
  std::vector<GdkContentProvider*> providers;
  if (data.text) {
    providers.push_back(
        gdk_content_provider_new_typed(G_TYPE_STRING, data.text->c_str()));
  }
  if (data.html) {
    auto* bytes = g_bytes_new(data.html->data(), data.html->size());
    providers.push_back(gdk_content_provider_new_for_bytes("text/html", bytes));
    g_bytes_unref(bytes);
  }
  if (data.image && !data.image->IsEmpty()) {
    auto* bitmap = data.image->ToSkBitmap();
    std::vector<guchar> pixels(size_t(bitmap->width()) * bitmap->height() * 4);
    for (int y = 0; y < bitmap->height(); ++y) {
      for (int x = 0; x < bitmap->width(); ++x) {
        auto color = bitmap->getColor(x, y);
        auto* p = pixels.data() + (size_t(y) * bitmap->width() + x) * 4;
        p[0] = SkColorGetR(color);
        p[1] = SkColorGetG(color);
        p[2] = SkColorGetB(color);
        p[3] = SkColorGetA(color);
      }
    }
    auto* bytes = g_bytes_new(pixels.data(), pixels.size());
    auto* texture = gdk_memory_texture_new(bitmap->width(), bitmap->height(),
                                           GDK_MEMORY_R8G8B8A8, bytes,
                                           size_t(bitmap->width()) * 4);
    providers.push_back(
        gdk_content_provider_new_typed(GDK_TYPE_TEXTURE, texture));
    g_object_unref(texture);
    g_bytes_unref(bytes);
  }
  // The union takes ownership of every child provider.
  auto* provider = providers.empty() ? nullptr
                                     : gdk_content_provider_new_union(
                                           providers.data(), providers.size());
  gdk_clipboard_set_content(clipboard, provider);
  if (provider) {
    g_object_unref(provider);
  }
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
