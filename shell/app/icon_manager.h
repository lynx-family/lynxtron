// Copyright 2021 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_APP_ICON_MANAGER_H_
#define LYNXTRON_SHELL_APP_ICON_MANAGER_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "ui/gfx/image/image.h"

namespace base {
class CancelableTaskTracker;
}

namespace lynxtron {

class IconManager {
 public:
  enum class IconSize { kSmall, kNormal, kLarge };

  IconManager();
  ~IconManager();

  IconManager(const IconManager&) = delete;
  IconManager& operator=(const IconManager&) = delete;

  static bool ParseIconSize(const std::string& size_string, IconSize* out);
  static int GetPixelSize(IconSize size);

  gfx::Image* LookupIconFromFilepath(const base::FilePath& path,
                                     IconSize size,
                                     float scale_factor);
  void LoadIcon(const base::FilePath& path,
                IconSize size,
                float scale_factor,
                base::OnceCallback<void(gfx::Image)> callback,
                base::CancelableTaskTracker* cancelable_task_tracker);

 private:
  struct IconKey {
    base::FilePath path;
    IconSize size;
    float scale_factor;

    bool operator<(const IconKey& other) const;
  };

  void OnIconLoaded(const IconKey& key,
                    base::OnceCallback<void(gfx::Image)> callback,
                    gfx::Image icon);

  std::map<IconKey, gfx::Image> cache_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_ICON_MANAGER_H_
