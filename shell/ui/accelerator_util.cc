// Copyright (c) 2026 Lynxtron Authors.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/ui/accelerator_util.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace accelerator_util {

namespace {

int ModifierFromToken(const std::string& token) {
  const std::string lower = base::ToLowerASCII(token);
  if (lower == "shift") {
    return ui::Accelerator::kShift;
  }
  if (lower == "ctrl" || lower == "control") {
    return ui::Accelerator::kCtrl;
  }
  if (lower == "alt" || lower == "option") {
    return ui::Accelerator::kAlt;
  }
  if (lower == "cmd" || lower == "command" || lower == "meta") {
    return ui::Accelerator::kCmd;
  }
  if (lower == "commandorcontrol" || lower == "cmdorctrl") {
#if defined(__APPLE__)
    return ui::Accelerator::kCmd;
#else
    return ui::Accelerator::kCtrl;
#endif
  }
  return 0;
}

}  // namespace

bool StringToAccelerator(const std::string& accelerator, ui::Accelerator* out) {
  if (!out) {
    return false;
  }

  if (accelerator.empty()) {
    *out = ui::Accelerator();
    return true;
  }

  const auto parts = base::SplitString(accelerator, "+", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_NONEMPTY);

  int modifiers = 0;
  std::string key;

  for (const auto& part : parts) {
    int modifier = ModifierFromToken(part);
    if (modifier != 0) {
      modifiers |= modifier;
    } else {
      key = part;
    }
  }

  if (key.empty()) {
    *out = ui::Accelerator();
    return false;
  }

  *out = ui::Accelerator(key, modifiers);
  return true;
}

}  // namespace accelerator_util
