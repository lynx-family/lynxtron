// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "api_tray.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "gin/converter.h"
#include "shell/api/api_menu.h"
#include "shell/api/api_native_image.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "shell/common/gin_converters/gfx_converter.h"
#include "shell/common/gin_converters/guid_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/error_thrower.h"
#include "shell/common/node_includes.h"

namespace lynxtron::api {

namespace {

bool ConvertTrayImage(v8::Isolate* isolate,
                      v8::Local<v8::Value> image_value,
                      gin_helper::ErrorThrower thrower,
                      gfx::Image* image_out) {
  base::FilePath icon_path;
  if (gin::ConvertFromV8(isolate, image_value, &icon_path)) {
    auto native_image = NativeImage::CreateFromPath(isolate, icon_path);
    if (native_image->image().IsEmpty()) {
#if BUILDFLAG(IS_WIN)
      const auto image_path = base::WideToUTF8(icon_path.value());
#else
      const auto image_path = icon_path.value();
#endif
      thrower.ThrowError("Failed to load image from path '" + image_path + "'");
      return false;
    }

    *image_out = native_image->image();
    return true;
  }

  NativeImage* native_image = nullptr;
  if (!gin::ConvertFromV8(isolate, image_value, &native_image) ||
      !native_image) {
    thrower.ThrowError("Argument must be a file path or a NativeImage");
    return false;
  }

  *image_out = native_image->image();
  return true;
}

}  // namespace

Tray* Tray::New(gin_helper::ErrorThrower thrower, gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  v8::Local<v8::Value> image;
  if (!args->GetNext(&image)) {
    thrower.ThrowError("image is required");
    return nullptr;
  }

  std::string guid;
  args->GetNext(&guid);
  if (!guid.empty()) {
    base::Uuid parsed_guid;
    if (!gin::ConvertFromV8(isolate, gin::ConvertToV8(isolate, guid),
                            &parsed_guid)) {
      thrower.ThrowError("Invalid GUID format");
      return nullptr;
    }
  }

  gfx::Image tray_image;
  if (!ConvertTrayImage(isolate, image, thrower, &tray_image)) {
    return nullptr;
  }

  auto* tray = new Tray();
  tray->tray_icon_ = TrayIcon::Create(tray, guid);
  if (!tray->tray_icon_) {
    delete tray;
    thrower.ThrowError("tray is not supported");
    return nullptr;
  }

  tray->tray_icon_->SetImage(tray_image);
  return tray;
}

Tray::Tray() = default;

Tray::~Tray() {
  Destroy();
}

void Tray::Destroy() {
  if (destroyed_) {
    return;
  }
  destroyed_ = true;
  tray_icon_.reset();
  menu_ = nullptr;
  menu_handle_.Reset();
  popup_menu_ = nullptr;
  popup_menu_handle_.Reset();
}

bool Tray::IsDestroyed() const {
  return destroyed_;
}

void Tray::SetImage(gin::Arguments* args) {
  if (!tray_icon_) {
    return;
  }
  v8::Isolate* isolate = args->isolate();
  v8::Local<v8::Value> image;
  if (!args->GetNext(&image)) {
    return;
  }
  gin_helper::ErrorThrower thrower(isolate);
  gfx::Image tray_image;
  if (!ConvertTrayImage(isolate, image, thrower, &tray_image)) {
    return;
  }
  tray_icon_->SetImage(tray_image);
}

void Tray::SetPressedImage(gin::Arguments* args) {
  if (!tray_icon_) {
    return;
  }
  v8::Isolate* isolate = args->isolate();
  v8::Local<v8::Value> image;
  if (!args->GetNext(&image)) {
    return;
  }
  gin_helper::ErrorThrower thrower(isolate);
  gfx::Image tray_image;
  if (!ConvertTrayImage(isolate, image, thrower, &tray_image)) {
    return;
  }
  tray_icon_->SetPressedImage(tray_image);
}

void Tray::SetToolTip(const std::string& tool_tip) {
  if (tray_icon_) {
    tray_icon_->SetToolTip(tool_tip);
  }
}

void Tray::SetTitle(gin::Arguments* args) {
  if (!tray_icon_) {
    return;
  }

  v8::Isolate* isolate = args->isolate();
  gin_helper::ErrorThrower thrower(isolate);

  std::string title;
  if (!args->GetNext(&title)) {
    args->ThrowError();
    return;
  }

  std::string font_type;
  v8::Local<v8::Value> options_value = args->PeekNext();
  if (!options_value.IsEmpty() && !options_value->IsUndefined()) {
    if (!options_value->IsObject()) {
      thrower.ThrowError("setTitle options must be an object");
      return;
    }
    args->Skip();
    gin_helper::Dictionary options_dict(isolate,
                                        options_value.As<v8::Object>());
    v8::Local<v8::Value> font_type_value;
    if (options_dict.Get("fontType", &font_type_value)) {
      if (!font_type_value->IsString() ||
          !gin::ConvertFromV8(isolate, font_type_value, &font_type) ||
          (font_type != "monospaced" && font_type != "monospacedDigit")) {
        thrower.ThrowError(
            "fontType must be one of 'monospaced' or 'monospacedDigit'");
        return;
      }
    }
  }

  tray_icon_->SetTitle(title, font_type);
}

std::string Tray::GetTitle() const {
  if (!tray_icon_) {
    return std::string();
  }
  return tray_icon_->GetTitle();
}

void Tray::SetIgnoreDoubleClickEvents(bool ignore) {
  if (tray_icon_) {
    tray_icon_->SetIgnoreDoubleClickEvents(ignore);
  }
}

bool Tray::GetIgnoreDoubleClickEvents() const {
  if (!tray_icon_) {
    return false;
  }
  return tray_icon_->GetIgnoreDoubleClickEvents();
}

Menu* Tray::GetMenuFromValue(v8::Isolate* isolate, v8::Local<v8::Value> menu) {
  if (menu->IsNullOrUndefined()) {
    return nullptr;
  }
  Menu* converted = nullptr;
  if (!gin::ConvertFromV8(isolate, menu, &converted)) {
    return nullptr;
  }
  return converted;
}

void Tray::SetMenu(Menu* menu,
                   v8::Isolate* isolate,
                   v8::Local<v8::Value> value) {
  menu_ = menu;
  menu_handle_.Reset();
  if (menu && !value.IsEmpty()) {
    menu_handle_.Reset(isolate, value.As<v8::Object>());
  }
}

void Tray::SetPopupMenu(Menu* menu,
                        v8::Isolate* isolate,
                        v8::Local<v8::Value> value) {
  popup_menu_ = menu;
  popup_menu_handle_.Reset();
  if (menu && !value.IsEmpty()) {
    popup_menu_handle_.Reset(isolate, value.As<v8::Object>());
  }
}

void Tray::SetContextMenu(gin::Arguments* args) {
  if (!tray_icon_) {
    return;
  }
  v8::Isolate* isolate = args->isolate();
  v8::Local<v8::Value> menu_value;
  if (!args->GetNext(&menu_value)) {
    return;
  }
  Menu* menu_obj = GetMenuFromValue(isolate, menu_value);
  SetMenu(menu_obj, isolate, menu_value);
  tray_icon_->SetContextMenu(menu_obj ? menu_obj->model() : nullptr);
}

void Tray::PopUpContextMenu(gin::Arguments* args) {
  if (!tray_icon_) {
    return;
  }
  v8::Isolate* isolate = args->isolate();
  gin_helper::ErrorThrower thrower(isolate);
  gfx::Point position;
  bool has_position = false;
  bool should_use_default_menu = true;

  Menu* menu_obj = nullptr;
  v8::Local<v8::Value> first_arg = args->PeekNext();
  if (!first_arg.IsEmpty()) {
    if (first_arg->IsNullOrUndefined()) {
      args->Skip();
      v8::Local<v8::Value> second_arg = args->PeekNext();
      if (!second_arg.IsEmpty() && !second_arg->IsUndefined()) {
        if (!args->GetNext(&position)) {
          thrower.ThrowError("Error processing argument at index 1");
          return;
        }
        has_position = true;
      }
    } else if (gin::ConvertFromV8(isolate, first_arg, &menu_obj) &&
               menu_obj != nullptr) {
      should_use_default_menu = false;
      args->Skip();
      SetPopupMenu(menu_obj, isolate, first_arg);
      v8::Local<v8::Value> second_arg = args->PeekNext();
      if (!second_arg.IsEmpty() && !second_arg->IsUndefined()) {
        if (!args->GetNext(&position)) {
          thrower.ThrowError("Error processing argument at index 1");
          return;
        }
        has_position = true;
      }
    } else if (args->GetNext(&position)) {
      has_position = true;
    } else {
      thrower.ThrowError("Error processing argument at index 0");
      return;
    }
  }

  if (should_use_default_menu) {
    SetPopupMenu(nullptr, isolate, v8::Local<v8::Value>());
  }
  if (!menu_obj) {
    menu_obj = menu_;
    if (menu_ && !menu_handle_.IsEmpty()) {
      SetPopupMenu(menu_, isolate, menu_handle_.Get(isolate));
    }
  }
  TrayPoint tray_position{0, 0};
  if (has_position) {
    tray_position = TrayPoint{position.x(), position.y()};
  }
  tray_icon_->PopUpContextMenu(menu_obj ? menu_obj->model() : nullptr,
                               tray_position);
}

void Tray::CloseContextMenu() {
  if (tray_icon_) {
    tray_icon_->CloseContextMenu();
  }
}

gfx::Rect Tray::GetBounds() const {
  if (!tray_icon_) {
    return gfx::Rect();
  }
  return ToGfxRect(tray_icon_->GetBounds());
}

v8::Local<v8::Value> Tray::GetGUID(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  if (!tray_icon_) {
    return v8::Null(isolate);
  }
  std::string guid = tray_icon_->GetGUID();
  if (guid.empty()) {
    return v8::Null(isolate);
  }
  return gin::StringToV8(isolate, guid);
}

void Tray::DisplayBalloon(gin::Arguments* args) {
  if (!tray_icon_) {
    return;
  }
  v8::Isolate* isolate = args->isolate();
  gin_helper::Dictionary options;
  if (!args->GetNext(&options)) {
    return;
  }
  TrayBalloonOptions balloon_options;
  std::string title;
  std::string content;
  std::string icon_type;
  bool large_icon = true;
  bool no_sound = false;
  bool respect_quiet_time = false;

  options.Get("title", &title);
  options.Get("content", &content);
  options.Get("iconType", &icon_type);
  options.Get("largeIcon", &large_icon);
  options.Get("noSound", &no_sound);
  options.Get("respectQuietTime", &respect_quiet_time);

  balloon_options.title = base::UTF8ToUTF16(title);
  balloon_options.content = base::UTF8ToUTF16(content);
  balloon_options.large_icon = large_icon;
  balloon_options.no_sound = no_sound;
  balloon_options.respect_quiet_time = respect_quiet_time;

  if (icon_type == "none") {
    balloon_options.icon_type = TrayBalloonIconType::kNone;
  } else if (icon_type == "info") {
    balloon_options.icon_type = TrayBalloonIconType::kInfo;
  } else if (icon_type == "warning") {
    balloon_options.icon_type = TrayBalloonIconType::kWarning;
  } else if (icon_type == "error") {
    balloon_options.icon_type = TrayBalloonIconType::kError;
  } else {
    balloon_options.icon_type = TrayBalloonIconType::kCustom;
  }

#if BUILDFLAG(IS_WIN)
  v8::Local<v8::Value> icon_value;
  if (options.Get("icon", &icon_value)) {
    NativeImage* icon_image = nullptr;
    if (NativeImage::TryConvertNativeImage(
            isolate, icon_value, &icon_image,
            NativeImage::OnConvertError::kThrow)) {
      int size = large_icon ? 32 : 16;
      balloon_options.icon = icon_image->GetHICON(size);
    }
  }
#endif

  tray_icon_->DisplayBalloon(balloon_options);
}

void Tray::RemoveBalloon() {
  if (tray_icon_) {
    tray_icon_->RemoveBalloon();
  }
}

void Tray::Focus() {
  if (tray_icon_) {
    tray_icon_->Focus();
  }
}

void Tray::FillObjectTemplate(v8::Isolate* isolate,
                              v8::Local<v8::ObjectTemplate> templ) {
  gin::ObjectTemplateBuilder(isolate, GetClassName(), templ)
      .SetMethod("destroy", &Tray::Destroy)
      .SetMethod("isDestroyed", &Tray::IsDestroyed)
      .SetMethod("setImage", &Tray::SetImage)
      .SetMethod("setPressedImage", &Tray::SetPressedImage)
      .SetMethod("setToolTip", &Tray::SetToolTip)
      .SetMethod("setTitle", &Tray::SetTitle)
      .SetMethod("getTitle", &Tray::GetTitle)
      .SetMethod("setIgnoreDoubleClickEvents",
                 &Tray::SetIgnoreDoubleClickEvents)
      .SetMethod("getIgnoreDoubleClickEvents",
                 &Tray::GetIgnoreDoubleClickEvents)
      .SetMethod("setContextMenu", &Tray::SetContextMenu)
      .SetMethod("popUpContextMenu", &Tray::PopUpContextMenu)
      .SetMethod("closeContextMenu", &Tray::CloseContextMenu)
      .SetMethod("getBounds", &Tray::GetBounds)
      .SetMethod("getGUID", &Tray::GetGUID)
      .SetMethod("displayBalloon", &Tray::DisplayBalloon)
      .SetMethod("removeBalloon", &Tray::RemoveBalloon)
      .SetMethod("focus", &Tray::Focus)
      .Build();
}

const gin::DeprecatedWrapperInfo* Tray::wrapper_info() const {
  return &kWrapperInfo;
}

const char* Tray::GetTypeName() {
  return "Tray";
}

void Tray::EmitClick(const TrayBounds& bounds, const TrayPoint& position) {
  Emit("click", ToGfxRect(bounds), ToGfxPoint(position));
}

void Tray::EmitMouseEvent(const char* name, const TrayPoint& position) {
  Emit(name, ToGfxPoint(position));
}

gfx::Rect Tray::ToGfxRect(const TrayBounds& bounds) const {
  return gfx::Rect(bounds.x, bounds.y, bounds.width, bounds.height);
}

gfx::Point Tray::ToGfxPoint(const TrayPoint& point) const {
  return gfx::Point(point.x, point.y);
}

void Tray::OnClick(const TrayBounds& bounds, const TrayPoint& position) {
  EmitClick(bounds, position);
}

void Tray::OnRightClick(const TrayBounds& bounds) {
  Emit("right-click", ToGfxRect(bounds));
}

void Tray::OnDoubleClick(const TrayBounds& bounds) {
  Emit("double-click", ToGfxRect(bounds));
}

void Tray::OnMiddleClick(const TrayBounds& bounds) {
  Emit("middle-click", ToGfxRect(bounds));
}

void Tray::OnMouseDown(const TrayPoint& position) {
  EmitMouseEvent("mouse-down", position);
}

void Tray::OnMouseUp(const TrayPoint& position) {
  EmitMouseEvent("mouse-up", position);
}

void Tray::OnMouseEnter(const TrayPoint& position) {
  EmitMouseEvent("mouse-enter", position);
}

void Tray::OnMouseLeave(const TrayPoint& position) {
  EmitMouseEvent("mouse-leave", position);
}

void Tray::OnMouseMove(const TrayPoint& position) {
  EmitMouseEvent("mouse-move", position);
}

void Tray::OnBalloonShow() {
  Emit("balloon-show");
}

void Tray::OnBalloonClick() {
  Emit("balloon-click");
}

void Tray::OnBalloonClosed() {
  Emit("balloon-closed");
}

void Tray::OnDrop() {
  Emit("drop");
}

void Tray::OnDropFiles(const std::vector<std::string>& files) {
  Emit("drop-files", files);
}

void Tray::OnDropText(const std::string& text) {
  Emit("drop-text", text);
}

void Tray::OnDragEnter() {
  Emit("drag-enter");
}

void Tray::OnDragLeave() {
  Emit("drag-leave");
}

void Tray::OnDragEnd() {
  Emit("drag-end");
}

gin::DeprecatedWrapperInfo Tray::kWrapperInfo = {gin::kEmbedderNativeGin};

}  // namespace lynxtron::api

namespace {

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  gin_helper::Dictionary dict(isolate, exports);
  dict.Set("Tray", lynxtron::api::Tray::GetConstructor(isolate, context));
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_binding_tray, Initialize)
