// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/platform_util.h"

#include <utility>

#include "base/functional/callback.h"

namespace platform_util {

void ShowItemInFolder(const base::FilePath& full_path) {}

void OpenPath(const base::FilePath& full_path, OpenCallback callback) {
  std::move(callback).Run("Opening paths is not implemented on Linux");
}

void OpenExternal(const GURL& url,
                  const OpenExternalOptions& options,
                  OpenCallback callback) {
  std::move(callback).Run("Opening external URLs is not implemented on Linux");
}

void Beep() {}

namespace internal {

bool PlatformTrashItem(const base::FilePath& full_path, std::string* error) {
  if (error) {
    *error = "Moving items to trash is not implemented on Linux";
  }
  return false;
}

}  // namespace internal

}  // namespace platform_util
