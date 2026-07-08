// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/ui/message_box.h"

#include <iostream>
#include <utility>

#include "base/functional/callback.h"

namespace lynxtron {

MessageBoxSettings::MessageBoxSettings() = default;
MessageBoxSettings::MessageBoxSettings(const MessageBoxSettings&) = default;
MessageBoxSettings::~MessageBoxSettings() = default;

int ShowMessageBoxSync(const MessageBoxSettings& settings) {
  return settings.cancel_id;
}

void ShowMessageBox(const MessageBoxSettings& settings,
                    MessageBoxCallback callback) {
  std::move(callback).Run(settings.cancel_id, settings.checkbox_checked);
}

void CloseMessageBox(int id) {}

void ShowErrorBox(const std::u16string& title, const std::u16string& content) {
  std::cerr << "Lynxtron error dialog is not implemented on Linux yet.\n";
}

}  // namespace lynxtron
