// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/ui/file_dialog.h"

#include "gin/dictionary.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/promise.h"

namespace file_dialog {

DialogSettings::DialogSettings() = default;
DialogSettings::DialogSettings(const DialogSettings&) = default;
DialogSettings::~DialogSettings() = default;

bool ShowOpenDialogSync(const DialogSettings& settings,
                        std::vector<base::FilePath>* paths) {
  return false;
}

void ShowOpenDialog(const DialogSettings& settings,
                    gin_helper::Promise<gin_helper::Dictionary> promise) {
  auto dict = gin::Dictionary::CreateEmpty(promise.isolate());
  dict.Set("canceled", true);
  dict.Set("filePaths", std::vector<base::FilePath>());
  promise.Resolve(dict);
}

std::optional<base::FilePath> ShowSaveDialogSync(
    const DialogSettings& settings) {
  return std::nullopt;
}

void ShowSaveDialog(const DialogSettings& settings,
                    gin_helper::Promise<gin_helper::Dictionary> promise) {
  auto dict = gin::Dictionary::CreateEmpty(promise.isolate());
  dict.Set("canceled", true);
  dict.Set("filePath", base::FilePath());
  promise.Resolve(dict);
}

}  // namespace file_dialog
