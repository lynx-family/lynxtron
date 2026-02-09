// Copyright (c) 2026 Lynxtron Authors.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_UI_ACCELERATOR_H_
#define LYNXTRON_SHELL_UI_ACCELERATOR_H_

#include <optional>
#include <string>

namespace ui {

class Accelerator {
 public:
  enum Modifier {
    kShift = 1 << 0,
    kCtrl = 1 << 1,
    kAlt = 1 << 2,
    kCmd = 1 << 3,
  };

  Accelerator();
  Accelerator(std::string key, int modifiers);
  ~Accelerator();

  bool IsEmpty() const;
  bool IsShiftDown() const;
  bool IsCtrlDown() const;
  bool IsAltDown() const;
  bool IsCmdDown() const;

  const std::string& key() const;
  int modifiers() const;
  std::u16string GetShortcutText() const;

  std::optional<char16_t> shifted_char;

 private:
  std::string key_;
  int modifiers_ = 0;
};

}  // namespace ui

#endif  // LYNXTRON_SHELL_UI_ACCELERATOR_H_
