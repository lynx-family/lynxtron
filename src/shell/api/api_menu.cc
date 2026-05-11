// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_menu.h"

#include <map>
#include <optional>
#include <utility>

#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "shell/api/api_base_window.h"
#include "shell/app/javascript_environment.h"
#include "shell/app/native_window.h"
#include "shell/app/window_list.h"
#include "shell/common/gin_converters/accelerator_converter.h"
#include "shell/common/gin_converters/callback_converter.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "shell/common/gin_converters/gurl_converter.h"
#include "shell/common/gin_converters/image_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/node_includes.h"
#include "shell/ui/events/event_constants.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

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
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  dict.SetMethod("setApplicationMenu", &Menu::SetApplicationMenu);
#endif
#if BUILDFLAG(IS_MAC)
  dict.SetMethod("sendActionToFirstResponder",
                 &Menu::SendActionToFirstResponder);
#endif
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_binding_menu, Initialize)

#if BUILDFLAG(IS_WIN)
namespace lynxtron::api {
namespace {

Menu* g_application_menu = nullptr;
HHOOK g_accelerator_hook = nullptr;
std::map<HWND, HMENU> g_window_menus;

int CurrentModifierFlags() {
  int modifiers = 0;
  if (::GetKeyState(VK_SHIFT) & 0x8000) {
    modifiers |= ui::Accelerator::kShift;
  }
  if (::GetKeyState(VK_CONTROL) & 0x8000) {
    modifiers |= ui::Accelerator::kCtrl;
  }
  if (::GetKeyState(VK_MENU) & 0x8000) {
    modifiers |= ui::Accelerator::kAlt;
  }
  if ((::GetKeyState(VK_LWIN) & 0x8000) || (::GetKeyState(VK_RWIN) & 0x8000)) {
    modifiers |= ui::Accelerator::kCmd;
  }
  return modifiers;
}

int CurrentEventFlags() {
  int flags = ui::EF_NONE;
  if (::GetKeyState(VK_SHIFT) & 0x8000) {
    flags |= ui::EF_SHIFT_DOWN;
  }
  if (::GetKeyState(VK_CONTROL) & 0x8000) {
    flags |= ui::EF_CONTROL_DOWN;
  }
  if (::GetKeyState(VK_MENU) & 0x8000) {
    flags |= ui::EF_ALT_DOWN;
  }
  if ((::GetKeyState(VK_LWIN) & 0x8000) || (::GetKeyState(VK_RWIN) & 0x8000)) {
    flags |= ui::EF_COMMAND_DOWN;
  }
  return flags;
}

std::optional<WPARAM> VirtualKeyFromAcceleratorKey(
    const std::string& accelerator_key) {
  const std::string key = base::ToUpperASCII(accelerator_key);
  if (key.size() == 1) {
    const char ch = key[0];
    if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z')) {
      return static_cast<WPARAM>(ch);
    }
    if (ch == '+') {
      return VK_OEM_PLUS;
    }
    if (ch == '-') {
      return VK_OEM_MINUS;
    }
  }

  if (key.size() >= 2 && key[0] == 'F') {
    int function_key = 0;
    for (size_t i = 1; i < key.size(); ++i) {
      if (key[i] < '0' || key[i] > '9') {
        function_key = 0;
        break;
      }
      function_key = function_key * 10 + key[i] - '0';
    }
    if (function_key >= 1 && function_key <= 24) {
      return VK_F1 + function_key - 1;
    }
  }

  if (key == "PLUS") {
    return VK_OEM_PLUS;
  }
  if (key == "MINUS") {
    return VK_OEM_MINUS;
  }
  if (key == "TAB") {
    return VK_TAB;
  }
  if (key == "ENTER" || key == "RETURN") {
    return VK_RETURN;
  }
  if (key == "ESC" || key == "ESCAPE") {
    return VK_ESCAPE;
  }
  if (key == "SPACE") {
    return VK_SPACE;
  }
  if (key == "BACKSPACE") {
    return VK_BACK;
  }
  if (key == "DELETE" || key == "DEL") {
    return VK_DELETE;
  }
  if (key == "INSERT" || key == "INS") {
    return VK_INSERT;
  }
  if (key == "HOME") {
    return VK_HOME;
  }
  if (key == "END") {
    return VK_END;
  }
  if (key == "PAGEUP") {
    return VK_PRIOR;
  }
  if (key == "PAGEDOWN") {
    return VK_NEXT;
  }
  if (key == "UP" || key == "ARROWUP") {
    return VK_UP;
  }
  if (key == "DOWN" || key == "ARROWDOWN") {
    return VK_DOWN;
  }
  if (key == "LEFT" || key == "ARROWLEFT") {
    return VK_LEFT;
  }
  if (key == "RIGHT" || key == "ARROWRIGHT") {
    return VK_RIGHT;
  }

  return std::nullopt;
}

bool AcceleratorMatchesKeyMessage(const ui::Accelerator& accelerator,
                                  WPARAM virtual_key,
                                  int modifiers) {
  if (accelerator.IsEmpty() || accelerator.modifiers() != modifiers) {
    return false;
  }

  std::optional<WPARAM> expected_key =
      VirtualKeyFromAcceleratorKey(accelerator.key());
  if (!expected_key) {
    return false;
  }
  if (*expected_key == VK_OEM_PLUS) {
    return virtual_key == VK_OEM_PLUS || virtual_key == VK_ADD;
  }
  if (*expected_key == VK_OEM_MINUS) {
    return virtual_key == VK_OEM_MINUS || virtual_key == VK_SUBTRACT;
  }

  return *expected_key == virtual_key;
}

std::wstring GetNativeMenuLabel(LynxtronMenuModel* model, size_t index) {
  std::wstring label = base::UTF16ToWide(model->GetLabelAt(index));
  ui::Accelerator accelerator;
  if (model->GetAcceleratorAtWithParams(index, true, &accelerator) &&
      !accelerator.IsEmpty()) {
    std::u16string shortcut = accelerator.GetShortcutText();
    if (!shortcut.empty()) {
      label.append(L"\t");
      label.append(base::UTF16ToWide(shortcut));
    }
  }
  return label;
}

bool ExecuteAcceleratorInModel(LynxtronMenuModel* model,
                               WPARAM virtual_key,
                               int modifiers,
                               int event_flags) {
  if (!model) {
    return false;
  }

  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    const bool visible = model->IsVisibleAt(i);
    if (!visible && !model->WorksWhenHiddenAt(i)) {
      continue;
    }

    if (model->GetTypeAt(i) == LynxtronMenuModel::TYPE_SUBMENU) {
      if (ExecuteAcceleratorInModel(model->GetSubmenuModelAt(i), virtual_key,
                                    modifiers, event_flags)) {
        return true;
      }
      continue;
    }

    if (!model->ShouldRegisterAcceleratorAt(i) || !model->IsEnabledAt(i)) {
      continue;
    }

    ui::Accelerator accelerator;
    if (!model->GetAcceleratorAtWithParams(i, true, &accelerator)) {
      continue;
    }

    if (AcceleratorMatchesKeyMessage(accelerator, virtual_key, modifiers)) {
      model->ActivatedAt(i, event_flags);
      return true;
    }
  }

  return false;
}

bool ExecuteCommandInModel(LynxtronMenuModel* model,
                           int command_id,
                           int event_flags) {
  if (!model) {
    return false;
  }

  if (std::optional<size_t> index = model->GetIndexOfCommandId(command_id)) {
    if (!model->IsEnabledAt(*index)) {
      return false;
    }
    model->ActivatedAt(*index, event_flags);
    return true;
  }

  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (model->GetTypeAt(i) == LynxtronMenuModel::TYPE_SUBMENU &&
        ExecuteCommandInModel(model->GetSubmenuModelAt(i), command_id,
                              event_flags)) {
      return true;
    }
  }

  return false;
}

bool ExecuteApplicationMenuAccelerator(WPARAM virtual_key) {
  if (!g_application_menu) {
    return false;
  }
  return ExecuteAcceleratorInModel(g_application_menu->model(), virtual_key,
                                   CurrentModifierFlags(), CurrentEventFlags());
}

LRESULT CALLBACK MenuAcceleratorHook(int code, WPARAM w_param, LPARAM l_param) {
  if (code >= 0 && w_param == PM_REMOVE && l_param != 0) {
    MSG* message = reinterpret_cast<MSG*>(l_param);
    if (message &&
        (message->message == WM_KEYDOWN || message->message == WM_SYSKEYDOWN) &&
        ExecuteApplicationMenuAccelerator(message->wParam)) {
      message->message = WM_NULL;
      message->wParam = 0;
      message->lParam = 0;
    }
  }
  return ::CallNextHookEx(g_accelerator_hook, code, w_param, l_param);
}

void EnsureAcceleratorHook() {
  if (g_accelerator_hook) {
    return;
  }
  g_accelerator_hook = ::SetWindowsHookExW(WH_GETMESSAGE, MenuAcceleratorHook,
                                           nullptr, ::GetCurrentThreadId());
}

HMENU BuildNativeMenuFromModel(LynxtronMenuModel* model, bool top_level) {
  HMENU menu = top_level ? ::CreateMenu() : ::CreatePopupMenu();
  if (!menu || !model) {
    return menu;
  }

  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (!model->IsVisibleAt(i)) {
      continue;
    }

    const int command_id = model->GetCommandIdAt(i);
    const auto type = model->GetTypeAt(i);
    if (type == LynxtronMenuModel::TYPE_SEPARATOR) {
      ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
      continue;
    }

    std::wstring label = GetNativeMenuLabel(model, i);
    if (type == LynxtronMenuModel::TYPE_SUBMENU) {
      HMENU sub_menu =
          BuildNativeMenuFromModel(model->GetSubmenuModelAt(i), false);
      ::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sub_menu),
                    label.c_str());
      continue;
    }

    UINT flags = MF_STRING;
    if (!model->IsEnabledAt(i)) {
      flags |= MF_GRAYED;
    }
    if (model->IsItemCheckedAt(i)) {
      flags |= MF_CHECKED;
    }
    ::AppendMenuW(menu, flags, static_cast<UINT_PTR>(command_id),
                  label.c_str());
  }

  return menu;
}

void SetWindowMenu(HWND hwnd, HMENU menu) {
  auto existing = g_window_menus.find(hwnd);
  if (existing != g_window_menus.end()) {
    ::SetMenu(hwnd, nullptr);
    ::DestroyMenu(existing->second);
    g_window_menus.erase(existing);
  }

  if (menu) {
    ::SetMenu(hwnd, menu);
    g_window_menus[hwnd] = menu;
  } else {
    ::SetMenu(hwnd, nullptr);
  }
  ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                     SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE |
                     SWP_NOZORDER);
  ::DrawMenuBar(hwnd);
  ::RedrawWindow(hwnd, nullptr, nullptr,
                 RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void ApplyApplicationMenuToWindows() {
  for (NativeWindow* window : WindowList::GetWindows()) {
    if (!window || window->IsClosed()) {
      continue;
    }
    HWND hwnd = window->GetNativeWindowHandle();
    if (!hwnd || !::IsWindow(hwnd)) {
      continue;
    }
    HMENU menu = g_application_menu ? BuildNativeMenuFromModel(
                                          g_application_menu->model(), true)
                                    : nullptr;
    SetWindowMenu(hwnd, menu);
  }
}

}  // namespace

Menu* Menu::New(gin::Arguments* args) {
  v8::Isolate* const isolate = args->isolate();
  Menu* const menu = new Menu(args);
  gin_helper::CallMethod(isolate, menu, "_init");
  return menu;
}

void Menu::SetApplicationMenu(Menu* menu) {
  g_application_menu = menu;
  EnsureAcceleratorHook();
  ApplyApplicationMenuToWindows();
}

bool Menu::ExecuteCommandFromApplicationMenu(int command_id, int event_flags) {
  if (!g_application_menu) {
    return false;
  }
  return ExecuteCommandInModel(g_application_menu->model(), command_id,
                               event_flags);
}

}  // namespace lynxtron::api
#endif
