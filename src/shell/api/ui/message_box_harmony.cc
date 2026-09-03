// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/ui/message_box.h"

#include <utility>

#include "base/functional/callback.h"

namespace lynxtron {

// HarmonyOS fallbacks for message box free functions and the
// MessageBoxSettings struct ctor/dtor. Declared in shell/api/ui/message_box.h
// (cross-platform, no #if guard) but defined only in
// message_box_mac.mm / message_box_win.cc.
//
// A native implementation can dispatch through OHOS promptAction APIs.

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

void ShowErrorBox(const std::u16string& title,
                  const std::u16string& content) {}

}  // namespace lynxtron
