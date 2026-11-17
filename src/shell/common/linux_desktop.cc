// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include "shell/common/linux_desktop.h"

#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "lynx/platform/embedder/public/lynx_windowless_renderer.h"
#include "shell/app/native_window.h"
#include "shell/app/window_list.h"
#include "shell/common/global_thread.h"
#include "shell/ui/lynxtron_menu_model.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace lynxtron {
namespace {
// X11 keycodes are evdev codes plus eight. HID usages preserve physical layout.
uint64_t PhysicalKey(guint code) {
  static constexpr uint16_t evdev_hid[] = {
      0,   41, 30, 31,  32, 33, 34,  35, 36,  37, 38,  39, 45, 46,  42, 43,
      20,  26, 8,  21,  23, 28, 24,  12, 18,  19, 47,  48, 40, 224, 4,  22,
      7,   9,  10, 11,  13, 14, 15,  51, 52,  53, 225, 49, 29, 27,  6,  25,
      5,   17, 16, 54,  55, 56, 229, 85, 226, 44, 57,  58, 59, 60,  61, 62,
      63,  64, 65, 66,  67, 83, 71,  95, 96,  97, 86,  92, 93, 94,  87, 89,
      90,  91, 98, 99,  0,  0,  0,   68, 69,  0,  0,   0,  0,  0,   0,  88,
      228, 84, 70, 230, 0,  74, 82,  75, 80,  79, 77,  81, 78, 73,  76};
  code = code >= 8 ? code - 8 : 0;
  if (code == 125) {
    return 0x700e3;
  }
  if (code == 126) {
    return 0x700e7;
  }
  return code < std::size(evdev_hid) && evdev_hid[code]
             ? 0x70000 | evdev_hid[code]
             : 0;
}
uint64_t LogicalKey(guint key) {
  const uint64_t plane = 0x100000000;
  switch (key) {
    case GDK_KEY_BackSpace:
      return plane | 8;
    case GDK_KEY_Tab:
    case GDK_KEY_ISO_Left_Tab:
      return plane | 9;
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
      return plane | 13;
    case GDK_KEY_Escape:
      return plane | 27;
    case GDK_KEY_Delete:
      return plane | 127;
    case GDK_KEY_Down:
      return plane | 0x301;
    case GDK_KEY_Left:
      return plane | 0x302;
    case GDK_KEY_Right:
      return plane | 0x303;
    case GDK_KEY_Up:
      return plane | 0x304;
    case GDK_KEY_End:
      return plane | 0x305;
    case GDK_KEY_Home:
      return plane | 0x306;
    case GDK_KEY_Page_Down:
      return plane | 0x307;
    case GDK_KEY_Page_Up:
      return plane | 0x308;
    case GDK_KEY_Shift_L:
      return 0x200000102;
    case GDK_KEY_Shift_R:
      return 0x200000103;
    case GDK_KEY_Control_L:
      return 0x200000100;
    case GDK_KEY_Control_R:
      return 0x200000101;
    case GDK_KEY_Alt_L:
      return 0x200000104;
    case GDK_KEY_Alt_R:
      return 0x200000105;
    case GDK_KEY_Meta_L:
    case GDK_KEY_Super_L:
      return 0x200000106;
    case GDK_KEY_Meta_R:
    case GDK_KEY_Super_R:
      return 0x200000107;
    default:
      if (key >= GDK_KEY_F1 && key <= GDK_KEY_F24) {
        return plane | (0x801 + key - GDK_KEY_F1);
      }
      return gdk_keyval_to_unicode(gdk_keyval_to_lower(key));
  }
}
class GtkRenderer : public lynx::pub::LynxWindowlessRenderer {
 public:
  explicit GtkRenderer(NativeWindow* window)
      : LynxWindowlessRenderer(kRendererTypeSoftware) {
    area_ = gtk_drawing_area_new();
    g_object_ref_sink(area_);
    gtk_widget_set_can_focus(area_, TRUE);
    gtk_widget_add_events(area_,
                          GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                              GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK |
                              GDK_SMOOTH_SCROLL_MASK | GDK_KEY_PRESS_MASK |
                              GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK);
    auto* content = GTK_BOX(g_object_get_data(
        G_OBJECT(window->GetNativeWindowHandle()), "lynxtron-content"));
    gtk_box_pack_end(content, area_, TRUE, TRUE, 0);
    im_ = gtk_im_multicontext_new();
    preedit_ = gtk_popover_new(area_);
    g_object_ref_sink(preedit_);
    gtk_popover_set_modal(GTK_POPOVER(preedit_), FALSE);
    gtk_widget_set_no_show_all(preedit_, TRUE);
    preedit_label_ = gtk_label_new("");
    gtk_container_add(GTK_CONTAINER(preedit_), preedit_label_);
    gtk_widget_show(preedit_label_);
    g_signal_connect(
        im_, "preedit-changed",
        G_CALLBACK(+[](GtkIMContext* context, gpointer data) {
          auto* self = static_cast<GtkRenderer*>(data);
          gchar* text = nullptr;
          PangoAttrList* attrs = nullptr;
          gint cursor;
          gtk_im_context_get_preedit_string(context, &text, &attrs, &cursor);
          gtk_label_set_text(GTK_LABEL(self->preedit_label_), text ? text : "");
          gtk_label_set_attributes(GTK_LABEL(self->preedit_label_), attrs);
          if (text && *text) {
            gtk_widget_show(self->preedit_);
          } else {
            gtk_widget_hide(self->preedit_);
          }
          if (attrs) {
            pango_attr_list_unref(attrs);
          }
          g_free(text);
        }),
        this);
    g_signal_connect(area_, "realize",
                     G_CALLBACK(+[](GtkWidget* area, gpointer data) {
                       auto* self = static_cast<GtkRenderer*>(data);
                       gtk_im_context_set_client_window(
                           self->im_, gtk_widget_get_window(area));
                     }),
                     this);
    g_signal_connect(
        area_, "draw",
        G_CALLBACK(
            +[](GtkWidget* area, cairo_t* cr, gpointer data) -> gboolean {
              auto* self = static_cast<GtkRenderer*>(data);
              base::AutoLock lock(self->pixels_lock_);
              if (self->pixels_.empty()) {
                return FALSE;
              }
              cairo_surface_t* surface = cairo_image_surface_create_for_data(
                  self->pixels_.data(), CAIRO_FORMAT_ARGB32, self->width_,
                  self->height_, self->width_ * 4);
              cairo_scale(
                  cr,
                  static_cast<double>(gtk_widget_get_allocated_width(area)) /
                      self->width_,
                  static_cast<double>(gtk_widget_get_allocated_height(area)) /
                      self->height_);
              cairo_set_source_surface(cr, surface, 0, 0);
              cairo_paint(cr);
              cairo_surface_destroy(surface);
              return TRUE;
            }),
        this);
    g_signal_connect(
        area_, "event",
        G_CALLBACK(+[](GtkWidget*, GdkEvent* event, gpointer data) -> gboolean {
          return static_cast<GtkRenderer*>(data)->HandleEvent(event);
        }),
        this);
    g_signal_connect(
        im_, "commit",
        G_CALLBACK(+[](GtkIMContext*, const gchar* text, gpointer data) {
          auto* self = static_cast<GtkRenderer*>(data);
          if (std::getenv("LYNXTRON_LINUX_INPUT_DEBUG")) {
            std::fprintf(stderr, "GTK IM commit: %s\n", text);
          }
          lynx_key_event_t event = {};
          event.struct_size = sizeof(event);
          event.timestamp = g_get_monotonic_time();
          event.type = kLynxKeyEventTypeDown;
          event.physical = 0;
          event.logical = g_utf8_get_char(text);
          event.character = text;
          event.synthesized = true;
          self->SendKeyEvent(&event);
          event.type = kLynxKeyEventTypeUp;
          event.character = nullptr;
          self->SendKeyEvent(&event);
        }),
        this);
    gtk_widget_show(area_);
    const char* renderer = std::getenv("LYNXTRON_LINUX_RENDERER");
    if (renderer && std::strcmp(renderer, "gl") == 0) {
      display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
      EGLint major, minor, count;
      const EGLint config[] = {EGL_SURFACE_TYPE,
                               EGL_PBUFFER_BIT,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_BIT,
                               EGL_RED_SIZE,
                               8,
                               EGL_GREEN_SIZE,
                               8,
                               EGL_BLUE_SIZE,
                               8,
                               EGL_ALPHA_SIZE,
                               8,
                               EGL_NONE};
      if (display_ != EGL_NO_DISPLAY &&
          eglInitialize(display_, &major, &minor) &&
          eglBindAPI(EGL_OPENGL_API) &&
          eglChooseConfig(display_, config, &config_, 1, &count) && count) {
        context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, nullptr);
        if (context_ != EGL_NO_CONTEXT) {
          type_ = kRendererTypeGLDirect;
          LOG(INFO) << "Linux renderer: OpenGL (EGL); driver is reported on "
                       "first frame";
        }
      }
      if (type_ != kRendererTypeGLDirect) {
        LOG(WARNING) << "EGL unavailable; falling back to software renderer";
      }
    }
  }
  ~GtkRenderer() override {
    if (context_ != EGL_NO_CONTEXT) {
      eglDestroyContext(display_, context_);
    }
    if (surface_ != EGL_NO_SURFACE) {
      eglDestroySurface(display_, surface_);
    }
    // EGL display is process-shared; one window must not terminate the others.
    g_signal_handlers_disconnect_by_data(area_, this);
    g_signal_handlers_disconnect_by_data(im_, this);
    gtk_im_context_set_client_window(im_, nullptr);
    g_object_unref(im_);
    gtk_widget_destroy(preedit_);
    g_object_unref(preedit_);
    gtk_widget_destroy(area_);
    g_object_unref(area_);
  }
  bool OnGLMakeCurrent() override {
    if (surface_ == EGL_NO_SURFACE) {
      const EGLint attributes[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
      surface_ = eglCreatePbufferSurface(display_, config_, attributes);
    }
    return eglMakeCurrent(display_, surface_, surface_, context_);
  }
  bool OnGLClearCurrent() override {
    return eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                          EGL_NO_CONTEXT);
  }
  uint32_t OnGLCreateFBO(int width, int height) override {
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192) {
      return 0;
    }
    if (width != gl_width_ || height != gl_height_) {
      eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (surface_ != EGL_NO_SURFACE) {
        eglDestroySurface(display_, surface_);
      }
      const EGLint attributes[] = {EGL_WIDTH, width, EGL_HEIGHT, height,
                                   EGL_NONE};
      surface_ = eglCreatePbufferSurface(display_, config_, attributes);
      eglMakeCurrent(display_, surface_, surface_, context_);
      gl_width_ = width;
      gl_height_ = height;
    }
    return 0;  // EGL pbuffer's default framebuffer.
  }
  void* OnGLProcResolver(const char* name) override {
    return reinterpret_cast<void*>(eglGetProcAddress(name));
  }
  bool OnGLPresent() override {
    if (gl_width_ <= 0 || gl_height_ <= 0) {
      return false;
    }
    if (!driver_logged_) {
      std::fprintf(stderr, "Linux GL driver: %s\n",
                   reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
      driver_logged_ = true;
    }
    std::vector<uint8_t> rgba(static_cast<size_t>(gl_width_) * gl_height_ * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, gl_width_, gl_height_, GL_RGBA, GL_UNSIGNED_BYTE,
                 rgba.data());
    if (glGetError() != GL_NO_ERROR) {
      return false;
    }
    base::AutoLock lock(pixels_lock_);
    width_ = gl_width_;
    height_ = gl_height_;
    pixels_.resize(rgba.size());
    for (size_t y = 0; y < height_; ++y) {
      for (size_t x = 0; x < width_; ++x) {
        const auto* source = &rgba[((height_ - 1 - y) * width_ + x) * 4];
        auto* target = &pixels_[(y * width_ + x) * 4];
        target[0] = source[2];
        target[1] = source[1];
        target[2] = source[0];
        target[3] = source[3];
      }
    }
    ScheduleDrawLocked();
    return true;
  }
  bool OnSoftwarePresent(const void* allocation,
                         size_t row_bytes,
                         size_t height) override {
    if (!allocation || !row_bytes || row_bytes % 4 || row_bytes / 4 > 8192 ||
        !height || height > 8192) {
      return false;
    }
    base::AutoLock lock(pixels_lock_);
    width_ = row_bytes / 4;
    height_ = height;
    pixels_.resize(row_bytes * height);
    // Cairo ARGB32 uses the same premultiplied native-endian N32 layout.
    static_assert(SK_R32_SHIFT == 16 && SK_B32_SHIFT == 0);
    std::memcpy(pixels_.data(), allocation, pixels_.size());
    ScheduleDrawLocked();
    return true;
  }
  void OnPostTask(lynx_task_t task, uint64_t interval) override {
    GlobalThread::GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](std::weak_ptr<LynxWindowlessRenderer> weak, lynx_task_t task) {
              if (auto self = weak.lock()) {
                self->RunTask(task);
              }
            },
            weak_from_this(), task),
        base::Nanoseconds(interval));
  }
  const char* GetClipboardData() override {
    auto* text =
        gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
    clipboard_ = text ? text : "";
    g_free(text);
    return clipboard_.c_str();
  }
  void SetClipboardData(const char* data) override {
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
                           data ? data : "", -1);
  }
  void ShowTextInput(bool show) override {
    if (std::getenv("LYNXTRON_LINUX_INPUT_DEBUG")) {
      std::fprintf(stderr, "GTK IM show: %d\n", show);
    }
    text_input_ = show;
    if (show) {
      gtk_im_context_focus_in(im_);
    } else {
      gtk_im_context_reset(im_);
      gtk_im_context_focus_out(im_);
    }
  }
  void UpdateCaretPosition(float x,
                           float y,
                           float width,
                           float height) override {
    GdkRectangle rect{static_cast<int>(x), static_cast<int>(y),
                      std::max(1, static_cast<int>(width)),
                      std::max(1, static_cast<int>(height))};
    gtk_im_context_set_cursor_location(im_, &rect);
    gtk_popover_set_pointing_to(GTK_POPOVER(preedit_), &rect);
  }
  void ActivateSystemCursor(lynx_cursor_type_e type, const char*) override {
    auto* window = gtk_widget_get_window(area_);
    if (!window) {
      return;
    }
    const char* name = "default";
    switch (type) {
      case kLynxCursorTypeNone: {
        auto* cursor = gdk_cursor_new_for_display(
            gdk_window_get_display(window), GDK_BLANK_CURSOR);
        gdk_window_set_cursor(window, cursor);
        g_object_unref(cursor);
        return;
      }
      case kLynxCursorTypeClick:
        name = "pointer";
        break;
      case kLynxCursorTypeForbidden:
        name = "not-allowed";
        break;
      case kLynxCursorTypeWait:
        name = "wait";
        break;
      case kLynxCursorTypeProgress:
        name = "progress";
        break;
      case kLynxCursorTypeContextmenu:
        name = "context-menu";
        break;
      case kLynxCursorTypeHelp:
        name = "help";
        break;
      case kLynxCursorTypeText:
        name = "text";
        break;
      case kLynxCursorTypeVerticalText:
        name = "vertical-text";
        break;
      case kLynxCursorTypeCell:
        name = "cell";
        break;
      case kLynxCursorTypePrecise:
        name = "crosshair";
        break;
      case kLynxCursorTypeMove:
        name = "move";
        break;
      case kLynxCursorTypeGrab:
        name = "grab";
        break;
      case kLynxCursorTypeGrabbing:
        name = "grabbing";
        break;
      case kLynxCursorTypeNoDrop:
        name = "no-drop";
        break;
      case kLynxCursorTypeAlias:
        name = "alias";
        break;
      case kLynxCursorTypeAllScroll:
        name = "all-scroll";
        break;
      case kLynxCursorTypeResizeLeftRight:
        name = "ew-resize";
        break;
      case kLynxCursorTypeResizeUpDown:
        name = "ns-resize";
        break;
      case kLynxCursorTypeResizeUpLeftDownRight:
        name = "nwse-resize";
        break;
      case kLynxCursorTypeResizeUpRightDownLeft:
        name = "nesw-resize";
        break;
      case kLynxCursorTypeResizeUp:
        name = "n-resize";
        break;
      case kLynxCursorTypeResizeDown:
        name = "s-resize";
        break;
      case kLynxCursorTypeResizeLeft:
        name = "w-resize";
        break;
      case kLynxCursorTypeResizeRight:
        name = "e-resize";
        break;
      case kLynxCursorTypeResizeUpLeft:
        name = "nw-resize";
        break;
      case kLynxCursorTypeResizeUpRight:
        name = "ne-resize";
        break;
      case kLynxCursorTypeResizeDownLeft:
        name = "sw-resize";
        break;
      case kLynxCursorTypeResizeDownRight:
        name = "se-resize";
        break;
      case kLynxCursorTypeResizeColumn:
        name = "col-resize";
        break;
      case kLynxCursorTypeResizeRow:
        name = "row-resize";
        break;
      case kLynxCursorTypeZoomIn:
        name = "zoom-in";
        break;
      case kLynxCursorTypeZoomOut:
        name = "zoom-out";
        break;
      default:
        break;
    }
    auto* cursor =
        gdk_cursor_new_from_name(gdk_window_get_display(window), name);
    gdk_window_set_cursor(window, cursor);
    if (cursor) {
      g_object_unref(cursor);
    }
  }

 private:
  // Called with pixels_lock_; retain at most one pending UI invalidation.
  void ScheduleDrawLocked() {
    if (dirty_) {
      return;
    }
    dirty_ = true;
    GlobalThread::GetUIThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](std::weak_ptr<LynxWindowlessRenderer> weak) {
                         if (auto self = weak.lock()) {
                           static_cast<GtkRenderer*>(self.get())->QueueDraw();
                         }
                       },
                       weak_from_this()));
  }
  void QueueDraw() {
    bool dirty;
    {
      base::AutoLock lock(pixels_lock_);
      dirty = dirty_;
      dirty_ = false;
    }
    if (dirty) {
      gtk_widget_queue_draw(area_);
    }
  }
  gboolean HandleEvent(GdkEvent* native) {
    if (native->type == GDK_KEY_PRESS || native->type == GDK_KEY_RELEASE) {
      auto& key = native->key;
      if (native->type == GDK_KEY_PRESS &&
          gtk_window_activate_key(GTK_WINDOW(gtk_widget_get_toplevel(area_)),
                                  &key)) {
        return TRUE;
      }
      bool filtered = text_input_ && gtk_im_context_filter_keypress(im_, &key);
      // IMEs can consume modifier releases after their press reached Clay.
      // Preserve release pairing so Ctrl/Shift cannot stay latched in
      // shortcuts.
      if (filtered && (native->type != GDK_KEY_RELEASE ||
                       !keys_.contains(PhysicalKey(key.hardware_keycode)))) {
        return TRUE;
      }
      lynx_key_event_t event = {};
      event.struct_size = sizeof(event);
      event.timestamp = g_get_monotonic_time();
      event.physical = PhysicalKey(key.hardware_keycode);
      event.logical = LogicalKey(key.keyval);
      if (!event.physical || !event.logical) {
        return FALSE;
      }
      if (native->type == GDK_KEY_PRESS) {
        event.type = keys_.emplace(event.physical, event.logical).second
                         ? kLynxKeyEventTypeDown
                         : kLynxKeyEventTypeRepeat;
        event.character = key.string;
      } else {
        event.type = kLynxKeyEventTypeUp;
        keys_.erase(event.physical);
      }
      SendKeyEvent(&event);
      return TRUE;
    }
    if (native->type == GDK_FOCUS_CHANGE) {
      if (native->focus_change.in) {
        gtk_im_context_focus_in(im_);
      } else {
        gtk_im_context_reset(im_);
        gtk_im_context_focus_out(im_);
        for (auto [physical, logical] : keys_) {
          lynx_key_event_t event = {};
          event.struct_size = sizeof(event);
          event.type = kLynxKeyEventTypeUp;
          event.physical = physical;
          event.logical = logical;
          event.synthesized = true;
          SendKeyEvent(&event);
        }
        keys_.clear();
        buttons_ = 0;
      }
      return FALSE;
    }
    lynx_pointer_event_t event = {};
    event.struct_size = sizeof(event);
    event.timestamp = g_get_monotonic_time();
    event.device_kind = kLynxPointerDeviceKindMouse;
    event.device = 0;
    gdouble x = 0, y = 0;
    if (!gdk_event_get_coords(native, &x, &y)) {
      return FALSE;
    }
    int scale = gtk_widget_get_scale_factor(area_);
    event.x = x * scale;
    event.y = y * scale;
    if (native->type == GDK_BUTTON_PRESS ||
        native->type == GDK_BUTTON_RELEASE) {
      guint button = native->button.button;
      int bit = button == 1 ? 1 : button == 2 ? 4 : button == 3 ? 2 : 0;
      if (!bit) {
        return FALSE;
      }
      if (native->type == GDK_BUTTON_PRESS) {
        gtk_widget_grab_focus(area_);
        buttons_ |= bit;
        event.phase = kLynxPointerPhaseDown;
      } else {
        buttons_ &= ~bit;
        event.phase = kLynxPointerPhaseUp;
      }
    } else if (native->type == GDK_MOTION_NOTIFY) {
      event.phase = buttons_ ? kLynxPointerPhaseMove : kLynxPointerPhaseHover;
    } else if (native->type == GDK_SCROLL) {
      event.phase = kLynxPointerPhaseHover;
      event.signal_kind = kLynxPointerSignalKindScroll;
      gdouble dx = 0, dy = 0;
      if (!gdk_event_get_scroll_deltas(native, &dx, &dy)) {
        dx = native->scroll.direction == GDK_SCROLL_LEFT    ? -1
             : native->scroll.direction == GDK_SCROLL_RIGHT ? 1
                                                            : 0;
        dy = native->scroll.direction == GDK_SCROLL_UP     ? -1
             : native->scroll.direction == GDK_SCROLL_DOWN ? 1
                                                           : 0;
      }
      event.scroll_delta_x = dx * 40 * scale;
      event.scroll_delta_y = dy * 40 * scale;
    } else {
      return FALSE;
    }
    event.buttons = buttons_;
    SendPointerEvent(&event);
    return TRUE;
  }
  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLConfig config_ = nullptr;
  EGLContext context_ = EGL_NO_CONTEXT;
  EGLSurface surface_ = EGL_NO_SURFACE;
  int gl_width_ = 0, gl_height_ = 0;
  bool driver_logged_ = false;
  GtkWidget* area_;
  GtkIMContext* im_;
  GtkWidget* preedit_;
  GtkWidget* preedit_label_;
  base::Lock pixels_lock_;
  std::vector<uint8_t> pixels_;
  size_t width_ = 0, height_ = 0;
  bool dirty_ = false, text_input_ = false;
  int buttons_ = 0;
  std::map<uint64_t, uint64_t> keys_;
  std::string clipboard_;
};
}  // namespace
uintptr_t GetLinuxNativeWindowId(NativeWindow* window) {
  if (!window || window->IsWindowless() || window->IsClosed()) {
    return 0;
  }
  auto* widget = GTK_WIDGET(window->GetNativeWindowHandle());
  gtk_widget_realize(widget);
  auto* native = gtk_widget_get_window(widget);
  return native && GDK_IS_X11_WINDOW(native) ? gdk_x11_window_get_xid(native)
                                             : 0;
}
void InitializeLinuxScreen();
void InitializeLinuxDesktop() {
  // GlobalThread's USE_GLIB UI message pump owns the default GLib context.
  // GTK and GIO callbacks run on that loop, without a polling timer.
  gtk_init_check(nullptr, nullptr);
  InitializeLinuxScreen();
}
bool EnsureLinuxDesktop() {
  bool ready = gtk_init_check(nullptr, nullptr);
  if (ready) {
    InitializeLinuxScreen();
  }
  return ready;
}
std::shared_ptr<lynx::pub::LynxWindowlessRenderer> CreateLinuxDesktopRenderer(
    NativeWindow* window) {
  return std::make_shared<GtkRenderer>(window);
}
}  // namespace lynxtron

namespace lynxtron {
namespace {
base::WeakPtr<LynxtronMenuModel> application_menu;
struct MenuItemData {
  base::WeakPtr<LynxtronMenuModel> model;
  size_t index;
};
}  // namespace
void* CreateLinuxMenu(LynxtronMenuModel* model, bool menubar) {
  GtkWidget* menu = menubar ? gtk_menu_bar_new() : gtk_menu_new();
  if (!model) {
    return menu;
  }
  for (size_t index = 0; index < model->GetItemCount(); ++index) {
    if (!model->IsVisibleAt(index)) {
      continue;
    }
    auto type = model->GetTypeAt(index);
    auto label = base::UTF16ToUTF8(model->GetLabelAt(index));
    auto* item = type == LynxtronMenuModel::TYPE_SEPARATOR
                     ? gtk_separator_menu_item_new()
                 : type == LynxtronMenuModel::TYPE_CHECK ||
                         type == LynxtronMenuModel::TYPE_RADIO
                     ? gtk_check_menu_item_new_with_label(label.c_str())
                     : gtk_menu_item_new_with_label(label.c_str());
    if (GTK_IS_CHECK_MENU_ITEM(item)) {
      gtk_check_menu_item_set_draw_as_radio(
          GTK_CHECK_MENU_ITEM(item), type == LynxtronMenuModel::TYPE_RADIO);
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
                                     model->IsItemCheckedAt(index));
    }
    gtk_widget_set_sensitive(item, model->IsEnabledAt(index));
    if (type == LynxtronMenuModel::TYPE_SUBMENU) {
      gtk_menu_item_set_submenu(
          GTK_MENU_ITEM(item),
          GTK_WIDGET(CreateLinuxMenu(model->GetSubmenuModelAt(index))));
    } else if (type != LynxtronMenuModel::TYPE_SEPARATOR) {
      auto* data = new MenuItemData{model->GetWeakPtr(), index};
      g_object_set_data(G_OBJECT(item), "lynxtron-menu-item", data);
      g_signal_connect_data(
          item, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer user) {
            auto* data = static_cast<MenuItemData*>(user);
            if (data->model) {
              data->model->ActivatedAt(data->index, 0);
            }
          }),
          data,
          +[](gpointer user, GClosure*) {
            delete static_cast<MenuItemData*>(user);
          },
          GConnectFlags(0));
    }
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  }
  gtk_widget_show_all(menu);
  return menu;
}
namespace {
void RegisterAccelerators(GtkWidget* menu, GtkAccelGroup* group) {
  GList* children = gtk_container_get_children(GTK_CONTAINER(menu));
  for (GList* child = children; child; child = child->next) {
    auto* item = GTK_WIDGET(child->data);
    if (auto* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item))) {
      RegisterAccelerators(submenu, group);
    }
    auto* data = static_cast<MenuItemData*>(
        g_object_get_data(G_OBJECT(item), "lynxtron-menu-item"));
    ui::Accelerator accelerator;
    if (!data || !data->model ||
        !data->model->ShouldRegisterAcceleratorAt(data->index) ||
        !data->model->GetAcceleratorAtWithParams(data->index, true,
                                                 &accelerator)) {
      continue;
    }
    std::string name = accelerator.key();
    if (name == "Space") {
      name = "space";
    } else if (name == "Enter") {
      name = "Return";
    } else if (name == "Backspace") {
      name = "BackSpace";
    } else if (name == "Plus") {
      name = "plus";
    }
    guint key = gdk_keyval_from_name(name.c_str());
    if (!key && name.size() == 1) {
      key = gdk_unicode_to_keyval(name[0]);
    }
    guint flags = 0;
    if (accelerator.IsCtrlDown()) {
      flags |= GDK_CONTROL_MASK;
    }
    if (accelerator.IsShiftDown()) {
      flags |= GDK_SHIFT_MASK;
    }
    if (accelerator.IsAltDown()) {
      flags |= GDK_MOD1_MASK;
    }
    if (accelerator.IsCmdDown()) {
      flags |= GDK_SUPER_MASK;
    }
    if (key) {
      gtk_widget_add_accelerator(item, "activate", group, key,
                                 GdkModifierType(flags), GTK_ACCEL_VISIBLE);
    }
  }
  g_list_free(children);
}
}  // namespace
void SetLinuxWindowMenu(NativeWindow* window, LynxtronMenuModel* model) {
  if (!window || window->IsWindowless() || window->IsClosed()) {
    return;
  }
  auto* widget = GTK_WIDGET(window->GetNativeWindowHandle());
  if (auto* group = GTK_ACCEL_GROUP(
          g_object_get_data(G_OBJECT(widget), "lynxtron-accel"))) {
    gtk_window_remove_accel_group(GTK_WINDOW(widget), group);
    g_object_set_data(G_OBJECT(widget), "lynxtron-accel", nullptr);
  }
  if (auto* old =
          GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "lynxtron-menu"))) {
    gtk_widget_destroy(old);
  }
  g_object_set_data(G_OBJECT(widget), "lynxtron-menu", nullptr);
  if (!model || !model->GetItemCount()) {
    return;
  }
  auto* menu = GTK_WIDGET(CreateLinuxMenu(model, true));
  auto* group = gtk_accel_group_new();
  RegisterAccelerators(menu, group);
  gtk_window_add_accel_group(GTK_WINDOW(widget), group);
  g_object_set_data(G_OBJECT(widget), "lynxtron-accel", group);
  g_object_unref(group);
  auto* content =
      GTK_BOX(g_object_get_data(G_OBJECT(widget), "lynxtron-content"));
  gtk_box_pack_start(content, menu, FALSE, FALSE, 0);
  gtk_box_reorder_child(content, menu, 0);
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
