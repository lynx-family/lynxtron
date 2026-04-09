// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_menu.h"

#include <utility>

#include "base/logging.h"
#include "base/notimplemented.h"
#include "build/build_config.h"
#include "shell/api/api_base_window.h"
#include "shell/app/javascript_environment.h"
#include "shell/common/gin_converters/accelerator_converter.h"
#include "shell/common/gin_converters/callback_converter.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "shell/common/gin_converters/gurl_converter.h"
#include "shell/common/gin_converters/image_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/node_includes.h"

#if BUILDFLAG(IS_MAC)
namespace gin {

using SharingItem = lynxtron::LynxtronMenuModel::SharingItem;

template <>
struct Converter<SharingItem> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     SharingItem* out) {
    gin_helper::Dictionary dict;
    if (!ConvertFromV8(isolate, val, &dict)) {
      return false;
    }
    dict.GetOptional("texts", &(out->texts));
    dict.GetOptional("filePaths", &(out->file_paths));
    dict.GetOptional("urls", &(out->urls));
    return true;
  }
};

}  // namespace gin
#endif

namespace lynxtron::api {

gin::DeprecatedWrapperInfo Menu::kWrapperInfo = {gin::kEmbedderNativeGin};

Menu::Menu(gin::Arguments* args)
    : model_(std::make_unique<LynxtronMenuModel>(this)) {
  model_->AddObserver(this);

#if BUILDFLAG(IS_MAC)
  gin_helper::Dictionary options;
  if (args->GetNext(&options)) {
    LynxtronMenuModel::SharingItem item;
    if (options.Get("sharingItem", &item)) {
      model_->SetSharingItem(std::move(item));
    }
  }
#endif
}

Menu::~Menu() {
  RemoveModelObserver();
}

void Menu::RemoveModelObserver() {
  if (model_) {
    model_->RemoveObserver(this);
  }
}

namespace {

v8::Local<v8::Value> CallMenuMethod(v8::Isolate* isolate,
                                    Menu* menu,
                                    const char* method) {
  return gin_helper::CallMethod(isolate, menu, method);
}

template <typename... Args>
v8::Local<v8::Value> CallMenuMethod(v8::Isolate* isolate,
                                    Menu* menu,
                                    const char* method,
                                    Args&&... args) {
  return gin_helper::CallMethod(isolate, menu, method,
                                std::forward<Args>(args)...);
}

bool InvokeBoolMethod(const Menu* menu,
                      const char* method,
                      int command_id,
                      bool default_value = false) {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Value> val =
      CallMenuMethod(isolate, const_cast<Menu*>(menu), method, command_id);
  bool ret = false;
  return gin::ConvertFromV8(isolate, val, &ret) ? ret : default_value;
}

}  // namespace

bool Menu::IsCommandIdChecked(int command_id) const {
  return InvokeBoolMethod(this, "_isCommandIdChecked", command_id);
}

bool Menu::IsCommandIdEnabled(int command_id) const {
  return InvokeBoolMethod(this, "_isCommandIdEnabled", command_id);
}

std::u16string Menu::GetLabelForCommandId(int command_id) const {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Value> val = CallMenuMethod(
      isolate, const_cast<Menu*>(this), "_getLabelForCommandId", command_id);
  std::u16string label;
  return gin::ConvertFromV8(isolate, val, &label) ? label : std::u16string();
}

std::u16string Menu::GetSecondaryLabelForCommandId(int command_id) const {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Value> val =
      CallMenuMethod(isolate, const_cast<Menu*>(this),
                     "_getSecondaryLabelForCommandId", command_id);
  std::u16string label;
  return gin::ConvertFromV8(isolate, val, &label) ? label : std::u16string();
}

gfx::Image Menu::GetIconForCommandId(int command_id) const {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Value> val = CallMenuMethod(isolate, const_cast<Menu*>(this),
                                            "_getIconForCommandId", command_id);
  gfx::Image icon;
  return gin::ConvertFromV8(isolate, val, &icon) ? icon : gfx::Image();
}

bool Menu::IsCommandIdVisible(int command_id) const {
  return InvokeBoolMethod(this, "_isCommandIdVisible", command_id);
}

bool Menu::ShouldCommandIdWorkWhenHidden(int command_id) const {
  return InvokeBoolMethod(this, "_shouldCommandIdWorkWhenHidden", command_id);
}

bool Menu::GetAcceleratorForCommandIdWithParams(
    int command_id,
    bool use_default_accelerator,
    ui::Accelerator* accelerator) const {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Value> val = CallMenuMethod(
      isolate, const_cast<Menu*>(this), "_getAcceleratorForCommandId",
      command_id, use_default_accelerator);
  return gin::ConvertFromV8(isolate, val, accelerator);
}

bool Menu::ShouldRegisterAcceleratorForCommandId(int command_id) const {
  return InvokeBoolMethod(this, "_shouldRegisterAcceleratorForCommandId",
                          command_id);
}

#if BUILDFLAG(IS_MAC)
bool Menu::GetSharingItemForCommandId(
    int command_id,
    LynxtronMenuModel::SharingItem* item) const {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Value> val =
      CallMenuMethod(isolate, const_cast<Menu*>(this),
                     "_getSharingItemForCommandId", command_id);
  return gin::ConvertFromV8(isolate, val, item);
}
#endif

void Menu::ExecuteCommand(int command_id, int event_flags) {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  gin_helper::Dictionary event = gin_helper::Dictionary::CreateEmpty(isolate);
  event.Set("flags", event_flags);
  CallMenuMethod(isolate, const_cast<Menu*>(this), "_executeCommand", event,
                 command_id);
}

void Menu::OnMenuWillShow() {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  CallMenuMethod(isolate, const_cast<Menu*>(this), "_menuWillShow");
  Emit("menu-will-show");
}

base::OnceClosure Menu::BindSelfToClosure(base::OnceClosure callback) {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> self;
  if (GetWrapper(isolate).ToLocal(&self)) {
    v8::Global<v8::Value> ref(isolate, self);
    return base::BindOnce(
        [](base::OnceClosure callback, v8::Global<v8::Value> ref) {
          std::move(callback).Run();
        },
        std::move(callback), std::move(ref));
  }
  return base::DoNothing();
}

void Menu::InsertItemAt(int index,
                        int command_id,
                        const std::u16string& label) {
  model_->InsertItemAt(index, command_id, label);
}

void Menu::InsertSeparatorAt(int index) {
  model_->InsertSeparatorAt(index);
}

void Menu::InsertCheckItemAt(int index,
                             int command_id,
                             const std::u16string& label) {
  model_->InsertCheckItemAt(index, command_id, label);
}

void Menu::InsertRadioItemAt(int index,
                             int command_id,
                             const std::u16string& label,
                             int group_id) {
  model_->InsertRadioItemAt(index, command_id, label, group_id);
}

void Menu::InsertSubMenuAt(int index,
                           int command_id,
                           const std::u16string& label,
                           Menu* menu) {
  menu->parent_ = this;
  model_->InsertSubMenuAt(index, command_id, label, menu->model_.get());
}

void Menu::SetIcon(int index, const gfx::Image& image) {
  model_->SetIcon(index, image);
}

void Menu::SetSublabel(int index, const std::u16string& sublabel) {
  model_->SetSecondaryLabel(index, sublabel);
}

void Menu::SetToolTip(int index, const std::u16string& toolTip) {
  model_->SetToolTip(index, toolTip);
}

void Menu::SetRole(int index, const std::u16string& role) {
  model_->SetRole(index, role);
}

void Menu::SetCustomType(int index, const std::u16string& customType) {
  model_->SetCustomType(index, customType);
}

void Menu::Clear() {
  model_->Clear();
}

int Menu::GetItemCount() const {
  return static_cast<int>(model_->GetItemCount());
}

int Menu::GetCommandIdAt(int index) const {
  return model_->GetCommandIdAt(index);
}

std::u16string Menu::GetLabelAt(int index) const {
  return model_->GetLabelAt(index);
}

std::u16string Menu::GetSublabelAt(int index) const {
  return model_->GetSecondaryLabelAt(index);
}

std::u16string Menu::GetToolTipAt(int index) const {
  return model_->GetToolTipAt(index);
}

std::u16string Menu::GetAcceleratorTextAtForTesting(int index) const {
  ui::Accelerator accelerator;
  model_->GetAcceleratorAtWithParams(index, true, &accelerator);
  return accelerator.GetShortcutText();
}

bool Menu::IsItemCheckedAt(int index) const {
  return model_->IsItemCheckedAt(index);
}

bool Menu::IsEnabledAt(int index) const {
  return model_->IsEnabledAt(index);
}

bool Menu::IsVisibleAt(int index) const {
  return model_->IsVisibleAt(index);
}

bool Menu::WorksWhenHiddenAt(int index) const {
  return model_->WorksWhenHiddenAt(index);
}

void Menu::OnMenuWillClose() {
  Emit("menu-will-close");
}

void Menu::PopupAt(BaseWindow* window,
                   int x,
                   int y,
                   int positioning_item,
                   base::OnceClosure callback) {
  (void)window;
  (void)x;
  (void)y;
  (void)positioning_item;
  (void)callback;
  NOTIMPLEMENTED();
}

void Menu::ClosePopupAt(int32_t window_id) {
  (void)window_id;
  NOTIMPLEMENTED();
}

void Menu::FillObjectTemplate(v8::Isolate* isolate,
                              v8::Local<v8::ObjectTemplate> templ) {
  gin::ObjectTemplateBuilder(isolate, GetClassName(), templ)
      .SetMethod("insertItem", &Menu::InsertItemAt)
      .SetMethod("insertCheckItem", &Menu::InsertCheckItemAt)
      .SetMethod("insertRadioItem", &Menu::InsertRadioItemAt)
      .SetMethod("insertSeparator", &Menu::InsertSeparatorAt)
      .SetMethod("insertSubMenu", &Menu::InsertSubMenuAt)
      .SetMethod("setIcon", &Menu::SetIcon)
      .SetMethod("setSublabel", &Menu::SetSublabel)
      .SetMethod("setToolTip", &Menu::SetToolTip)
      .SetMethod("setRole", &Menu::SetRole)
      .SetMethod("setCustomType", &Menu::SetCustomType)
      .SetMethod("clear", &Menu::Clear)
      .SetMethod("getItemCount", &Menu::GetItemCount)
      .SetMethod("getCommandIdAt", &Menu::GetCommandIdAt)
      .SetMethod("getLabelAt", &Menu::GetLabelAt)
      .SetMethod("getSublabelAt", &Menu::GetSublabelAt)
      .SetMethod("getToolTipAt", &Menu::GetToolTipAt)
      .SetMethod("isItemCheckedAt", &Menu::IsItemCheckedAt)
      .SetMethod("isEnabledAt", &Menu::IsEnabledAt)
      .SetMethod("worksWhenHiddenAt", &Menu::WorksWhenHiddenAt)
      .SetMethod("isVisibleAt", &Menu::IsVisibleAt)
      .SetMethod("popupAt", &Menu::PopupAt)
      .SetMethod("closePopupAt", &Menu::ClosePopupAt)
      .SetMethod("_getAcceleratorTextAt", &Menu::GetAcceleratorTextAtForTesting)
#if BUILDFLAG(IS_MAC)
      .SetMethod("_getUserAcceleratorAt", &Menu::GetUserAcceleratorAt)
#endif
      .Build();
}

const gin::DeprecatedWrapperInfo* Menu::wrapper_info() const {
  return &kWrapperInfo;
}

const char* Menu::GetTypeName() {
  return "Menu";
}

}  // namespace lynxtron::api

namespace {

using lynxtron::api::Menu;

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* const isolate = lynxtron::JavascriptEnvironment::GetIsolate();
  gin_helper::Dictionary dict{isolate, exports};
  dict.Set("Menu", Menu::GetConstructor(isolate, context));
#if BUILDFLAG(IS_MAC)
  dict.SetMethod("setApplicationMenu", &Menu::SetApplicationMenu);
  dict.SetMethod("sendActionToFirstResponder",
                 &Menu::SendActionToFirstResponder);
#endif
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_binding_menu, Initialize)

#if BUILDFLAG(IS_WIN)
namespace lynxtron::api {

Menu* Menu::New(gin::Arguments* args) {
  v8::Isolate* const isolate = args->isolate();
  Menu* const menu = new Menu(args);
  gin_helper::CallMethod(isolate, menu, "_init");
  return menu;
}

}  // namespace lynxtron::api
#endif
