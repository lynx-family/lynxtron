// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_DIALOG_HELPER_H_
#define SRC_DIALOG_HELPER_H_

#include <cstddef>
#include <string>

namespace dialog_helper {

struct DialogInfo {
  std::string type;
  std::string buttons;
  std::string message;
  std::string detail;
  std::string checkbox_label;
  bool checkbox_checked = false;

  std::string prompt;
  std::string panel_message;
  std::string directory;

  std::string name_field_label;
  std::string name_field_value;
  bool shows_tag_field = true;

  bool can_choose_files = false;
  bool can_choose_directories = false;
  bool allows_multiple_selection = false;

  bool shows_hidden_files = false;
  bool resolves_aliases = true;
  bool treats_packages_as_directories = false;
  bool can_create_directories = false;
};

DialogInfo GetDialogInfo(char* handle, size_t size);
bool ClickMessageBoxButton(char* handle, size_t size, int button_index);
bool ClickCheckbox(char* handle, size_t size);
bool CancelFileDialog(char* handle, size_t size);
bool AcceptFileDialog(char* handle, size_t size, const std::string& filename);

}  // namespace dialog_helper

#endif  // SRC_DIALOG_HELPER_H_
