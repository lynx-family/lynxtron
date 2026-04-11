// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <js_native_api.h>
#include <node_api.h>

#include <string>

#include "dialog_helper.h"

namespace {

bool GetHandleArg(napi_env env,
                  napi_callback_info info,
                  size_t expected_argc,
                  napi_value* args,
                  char** data,
                  size_t* length) {
  size_t argc = expected_argc;
  napi_status status =
      napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (status != napi_ok || argc < 1) {
    return false;
  }

  bool is_buffer;
  status = napi_is_buffer(env, args[0], &is_buffer);
  if (status != napi_ok || !is_buffer) {
    napi_throw_error(env, nullptr,
                     "First argument must be a Buffer (native window handle)");
    return false;
  }

  status = napi_get_buffer_info(env, args[0], reinterpret_cast<void**>(data),
                                length);
  return status == napi_ok;
}

napi_value GetDialogInfo(napi_env env, napi_callback_info info) {
  napi_value args[1];
  char* data;
  size_t length;
  if (!GetHandleArg(env, info, 1, args, &data, &length)) {
    return nullptr;
  }

  dialog_helper::DialogInfo di = dialog_helper::GetDialogInfo(data, length);

  napi_value result;
  napi_create_object(env, &result);

  napi_value type_val;
  napi_create_string_utf8(env, di.type.c_str(), di.type.size(), &type_val);
  napi_set_named_property(env, result, "type", type_val);

  napi_value buttons_val;
  napi_create_string_utf8(env, di.buttons.c_str(), di.buttons.size(),
                          &buttons_val);
  napi_set_named_property(env, result, "buttons", buttons_val);

  napi_value message_val;
  napi_create_string_utf8(env, di.message.c_str(), di.message.size(),
                          &message_val);
  napi_set_named_property(env, result, "message", message_val);

  napi_value detail_val;
  napi_create_string_utf8(env, di.detail.c_str(), di.detail.size(),
                          &detail_val);
  napi_set_named_property(env, result, "detail", detail_val);

  napi_value checkbox_label_val;
  napi_create_string_utf8(env, di.checkbox_label.c_str(),
                          di.checkbox_label.size(), &checkbox_label_val);
  napi_set_named_property(env, result, "checkboxLabel", checkbox_label_val);

  napi_value checkbox_checked_val;
  napi_get_boolean(env, di.checkbox_checked, &checkbox_checked_val);
  napi_set_named_property(env, result, "checkboxChecked", checkbox_checked_val);

  napi_value prompt_val;
  napi_create_string_utf8(env, di.prompt.c_str(), di.prompt.size(),
                          &prompt_val);
  napi_set_named_property(env, result, "prompt", prompt_val);

  napi_value panel_message_val;
  napi_create_string_utf8(env, di.panel_message.c_str(),
                          di.panel_message.size(), &panel_message_val);
  napi_set_named_property(env, result, "panelMessage", panel_message_val);

  napi_value directory_val;
  napi_create_string_utf8(env, di.directory.c_str(), di.directory.size(),
                          &directory_val);
  napi_set_named_property(env, result, "directory", directory_val);

  napi_value name_field_label_val;
  napi_create_string_utf8(env, di.name_field_label.c_str(),
                          di.name_field_label.size(), &name_field_label_val);
  napi_set_named_property(env, result, "nameFieldLabel", name_field_label_val);

  napi_value name_field_value_val;
  napi_create_string_utf8(env, di.name_field_value.c_str(),
                          di.name_field_value.size(), &name_field_value_val);
  napi_set_named_property(env, result, "nameFieldValue", name_field_value_val);

  napi_value shows_tag_field_val;
  napi_get_boolean(env, di.shows_tag_field, &shows_tag_field_val);
  napi_set_named_property(env, result, "showsTagField", shows_tag_field_val);

  napi_value can_choose_files_val;
  napi_get_boolean(env, di.can_choose_files, &can_choose_files_val);
  napi_set_named_property(env, result, "canChooseFiles", can_choose_files_val);

  napi_value can_choose_dirs_val;
  napi_get_boolean(env, di.can_choose_directories, &can_choose_dirs_val);
  napi_set_named_property(env, result, "canChooseDirectories",
                          can_choose_dirs_val);

  napi_value allows_multi_val;
  napi_get_boolean(env, di.allows_multiple_selection, &allows_multi_val);
  napi_set_named_property(env, result, "allowsMultipleSelection",
                          allows_multi_val);

  napi_value shows_hidden_val;
  napi_get_boolean(env, di.shows_hidden_files, &shows_hidden_val);
  napi_set_named_property(env, result, "showsHiddenFiles", shows_hidden_val);

  napi_value resolves_aliases_val;
  napi_get_boolean(env, di.resolves_aliases, &resolves_aliases_val);
  napi_set_named_property(env, result, "resolvesAliases", resolves_aliases_val);

  napi_value treats_packages_val;
  napi_get_boolean(env, di.treats_packages_as_directories,
                   &treats_packages_val);
  napi_set_named_property(env, result, "treatsPackagesAsDirectories",
                          treats_packages_val);

  napi_value can_create_dirs_val;
  napi_get_boolean(env, di.can_create_directories, &can_create_dirs_val);
  napi_set_named_property(env, result, "canCreateDirectories",
                          can_create_dirs_val);

  return result;
}

napi_value ClickMessageBoxButton(napi_env env, napi_callback_info info) {
  napi_value args[2];
  char* data;
  size_t length;
  if (!GetHandleArg(env, info, 2, args, &data, &length)) {
    return nullptr;
  }

  int32_t button_index;
  napi_status status = napi_get_value_int32(env, args[1], &button_index);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr,
                     "Second argument must be a number (button index)");
    return nullptr;
  }

  bool ok = dialog_helper::ClickMessageBoxButton(data, length, button_index);

  napi_value result;
  napi_get_boolean(env, ok, &result);
  return result;
}

napi_value ClickCheckbox(napi_env env, napi_callback_info info) {
  napi_value args[1];
  char* data;
  size_t length;
  if (!GetHandleArg(env, info, 1, args, &data, &length)) {
    return nullptr;
  }

  bool ok = dialog_helper::ClickCheckbox(data, length);

  napi_value result;
  napi_get_boolean(env, ok, &result);
  return result;
}

napi_value CancelFileDialog(napi_env env, napi_callback_info info) {
  napi_value args[1];
  char* data;
  size_t length;
  if (!GetHandleArg(env, info, 1, args, &data, &length)) {
    return nullptr;
  }

  bool ok = dialog_helper::CancelFileDialog(data, length);

  napi_value result;
  napi_get_boolean(env, ok, &result);
  return result;
}

napi_value AcceptFileDialog(napi_env env, napi_callback_info info) {
  napi_value args[2];
  char* data;
  size_t length;
  if (!GetHandleArg(env, info, 2, args, &data, &length)) {
    return nullptr;
  }

  std::string filename;
  napi_valuetype value_type = napi_undefined;
  if (napi_typeof(env, args[1], &value_type) == napi_ok &&
      value_type == napi_string) {
    size_t str_len = 0;
    napi_get_value_string_utf8(env, args[1], nullptr, 0, &str_len);
    filename.resize(str_len);
    napi_get_value_string_utf8(env, args[1], filename.data(), str_len + 1,
                               &str_len);
  }

  bool ok = dialog_helper::AcceptFileDialog(data, length, filename);

  napi_value result;
  napi_get_boolean(env, ok, &result);
  return result;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
      {"getDialogInfo", nullptr, GetDialogInfo, nullptr, nullptr, nullptr,
       napi_enumerable, nullptr},
      {"clickMessageBoxButton", nullptr, ClickMessageBoxButton, nullptr,
       nullptr, nullptr, napi_enumerable, nullptr},
      {"clickCheckbox", nullptr, ClickCheckbox, nullptr, nullptr, nullptr,
       napi_enumerable, nullptr},
      {"cancelFileDialog", nullptr, CancelFileDialog, nullptr, nullptr, nullptr,
       napi_enumerable, nullptr},
      {"acceptFileDialog", nullptr, AcceptFileDialog, nullptr, nullptr, nullptr,
       napi_enumerable, nullptr},
  };

  napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors);
  return exports;
}

}  // namespace

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
