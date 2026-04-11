// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
//
// JS binding for clipboard module.

#include "shell/api/api_clipboard.h"

#include <string>
#include <vector>

#include "build/build_config.h"
#include "gin/converter.h"
#include "shell/api/api_native_image.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/error_thrower.h"
#include "shell/common/gin_helper/handle.h"
#include "shell/common/node_includes.h"

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
namespace lynxtron::api::clipboard {

std::vector<std::string> AvailableFormats() {
  return {};
}

void Clear() {}

std::string ReadHTML() {
  return {};
}

gfx::Image ReadImage() {
  return {};
}

std::string ReadText() {
  return {};
}

void Write(const ClipboardData& data) {}

void WriteHTML(const std::string& markup) {}

void WriteImage(const gfx::Image& image) {}

void WriteText(const std::string& text) {}

}  // namespace lynxtron::api::clipboard
#endif

namespace {

std::string NormalizeHtml(const std::string& markup) {
  if (markup.find("<meta charset=") != std::string::npos) {
    return markup;
  }
  return std::string("<meta charset='utf-8'>") + markup;
}

v8::Local<v8::Value> AvailableFormats(v8::Isolate* isolate) {
  const std::vector<std::string> formats =
      lynxtron::api::clipboard::AvailableFormats();
  return gin::ConvertToV8(isolate, formats);
}

void Clear() {
  lynxtron::api::clipboard::Clear();
}

std::string ReadHTML() {
  return lynxtron::api::clipboard::ReadHTML();
}

v8::Local<v8::Value> ReadImage(v8::Isolate* isolate) {
  auto image = lynxtron::api::clipboard::ReadImage();
  auto handle = lynxtron::api::NativeImage::Create(isolate, image);
  return handle.ToV8();
}

std::string ReadText() {
  return lynxtron::api::clipboard::ReadText();
}

void Write(gin_helper::ErrorThrower thrower, v8::Local<v8::Value> data_val) {
  if (!data_val->IsObject()) {
    thrower.ThrowError("data must be an object");
    return;
  }
  v8::Local<v8::Object> data_obj = data_val.As<v8::Object>();
  gin_helper::Dictionary dict(thrower.isolate(), data_obj);
  lynxtron::api::clipboard::ClipboardData data;
  std::string s;
  if (dict.Get("text", &s)) {
    data.text = s;
  }
  s.clear();
  if (dict.Get("html", &s)) {
    data.html = NormalizeHtml(s);
  }
  v8::Local<v8::Value> img_val;
  if (dict.Get("image", &img_val) && !img_val->IsNullOrUndefined()) {
    lynxtron::api::NativeImage* native_img = nullptr;
    if (!lynxtron::api::NativeImage::TryConvertNativeImage(
            thrower.isolate(), img_val, &native_img,
            lynxtron::api::NativeImage::OnConvertError::kThrow)) {
      return;
    }
    data.image = native_img->image();
  }
  lynxtron::api::clipboard::Write(data);
}

void WriteHTML(const std::string& markup) {
  lynxtron::api::clipboard::WriteHTML(NormalizeHtml(markup));
}

void WriteImage(gin_helper::ErrorThrower thrower,
                v8::Local<v8::Value> img_val) {
  lynxtron::api::NativeImage* native_img = nullptr;
  if (!lynxtron::api::NativeImage::TryConvertNativeImage(
          thrower.isolate(), img_val, &native_img,
          lynxtron::api::NativeImage::OnConvertError::kThrow)) {
    return;
  }
  lynxtron::api::clipboard::WriteImage(native_img->image());
}

void WriteText(const std::string& text) {
  lynxtron::api::clipboard::WriteText(text);
}

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* const isolate = v8::Isolate::GetCurrent();
  gin_helper::Dictionary dict{isolate, exports};
  dict.SetMethod("availableFormats", &AvailableFormats);
  dict.SetMethod("clear", &Clear);
  dict.SetMethod("readHTML", &ReadHTML);
  dict.SetMethod("readImage", &ReadImage);
  dict.SetMethod("readText", &ReadText);
  dict.SetMethod("write", &Write);
  dict.SetMethod("writeHTML", &WriteHTML);
  dict.SetMethod("writeImage", &WriteImage);
  dict.SetMethod("writeText", &WriteText);
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_binding_clipboard, Initialize)
