// Copyright (c) 2026 Lynxtron Authors.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/ui/accelerator.h"

#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace ui {

Accelerator::Accelerator() = default;

Accelerator::Accelerator(std::string key, int modifiers)
    : key_(std::move(key)), modifiers_(modifiers) {}

Accelerator::~Accelerator() = default;

bool Accelerator::IsEmpty() const {
  return key_.empty();
}

bool Accelerator::IsShiftDown() const {
  return modifiers_ & kShift;
}

bool Accelerator::IsCtrlDown() const {
  return modifiers_ & kCtrl;
}

bool Accelerator::IsAltDown() const {
  return modifiers_ & kAlt;
}

bool Accelerator::IsCmdDown() const {
  return modifiers_ & kCmd;
}

const std::string& Accelerator::key() const {
  return key_;
}

int Accelerator::modifiers() const {
  return modifiers_;
}

std::u16string Accelerator::GetShortcutText() const {
  std::u16string text;
  auto append = [&](const std::u16string& part) {
    if (!text.empty()) {
      text += u"+";
    }
    text += part;
  };

  if (IsCtrlDown()) {
    append(u"Ctrl");
  }
  if (IsShiftDown()) {
    append(u"Shift");
  }
  if (IsAltDown()) {
    append(u"Alt");
  }
  if (IsCmdDown()) {
    append(u"Command");
  }

  std::u16string key_text = base::UTF8ToUTF16(key_);
  if (!key_text.empty()) {
    for (auto& ch : key_text) {
      ch = base::ToUpperASCII(ch);
    }
    append(key_text);
  }

  return text;
}

}  // namespace ui
