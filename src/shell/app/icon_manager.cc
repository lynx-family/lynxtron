// Copyright 2021 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/icon_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "shell/common/global_thread.h"

namespace lynxtron {
gfx::Image LoadPlatformIcon(const base::FilePath& path,
                            IconManager::IconSize size,
                            float scale_factor);

IconManager::IconManager() = default;

IconManager::~IconManager() = default;

bool IconManager::ParseIconSize(const std::string& size_string, IconSize* out) {
  if (size_string == "small") {
    *out = IconSize::kSmall;
    return true;
  }
  if (size_string == "normal") {
    *out = IconSize::kNormal;
    return true;
  }
  if (size_string == "large") {
    *out = IconSize::kLarge;
    return true;
  }
  return false;
}

int IconManager::GetPixelSize(IconSize size) {
  switch (size) {
    case IconSize::kSmall:
      return 16;
    case IconSize::kLarge:
      return 256;
    case IconSize::kNormal:
      return 32;
  }
}

bool IconManager::IconKey::operator<(const IconKey& other) const {
  if (path != other.path) {
    return path < other.path;
  }
  if (size != other.size) {
    return static_cast<int>(size) < static_cast<int>(other.size);
  }
  return scale_factor < other.scale_factor;
}

gfx::Image* IconManager::LookupIconFromFilepath(const base::FilePath& path,
                                                IconSize size,
                                                float scale_factor) {
  IconKey key{path.NormalizePathSeparators(), size, scale_factor};
  auto it = cache_.find(key);
  if (it == cache_.end()) {
    return nullptr;
  }
  return &it->second;
}

void IconManager::LoadIcon(
    const base::FilePath& path,
    IconSize size,
    float scale_factor,
    base::OnceCallback<void(gfx::Image)> callback,
    base::CancelableTaskTracker* cancelable_task_tracker) {
  IconKey key{path.NormalizePathSeparators(), size, scale_factor};
  if (auto* cached = LookupIconFromFilepath(key.path, size, scale_factor)) {
    std::move(callback).Run(*cached);
    return;
  }

#if BUILDFLAG(IS_MAC)
  // AppKit APIs used by LoadPlatformIcon (NSWorkspace/NSImage) require the UI
  // thread on macOS.
  auto task_runner = GetUIThreadTaskRunner();
  if (task_runner) {
    cancelable_task_tracker->PostTaskAndReplyWithResult(
        task_runner.get(), FROM_HERE,
        base::BindOnce(&LoadPlatformIcon, key.path, size, scale_factor),
        base::BindOnce(&IconManager::OnIconLoaded, base::Unretained(this), key,
                       std::move(callback)));
  } else {
    OnIconLoaded(key, std::move(callback),
                 LoadPlatformIcon(key.path, size, scale_factor));
  }
#else
  scoped_refptr<base::TaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  cancelable_task_tracker->PostTaskAndReplyWithResult(
      task_runner.get(), FROM_HERE,
      base::BindOnce(&LoadPlatformIcon, key.path, size, scale_factor),
      base::BindOnce(&IconManager::OnIconLoaded, base::Unretained(this), key,
                     std::move(callback)));
#endif
}

void IconManager::OnIconLoaded(const IconKey& key,
                               base::OnceCallback<void(gfx::Image)> callback,
                               gfx::Image icon) {
  if (!icon.IsEmpty()) {
    cache_[key] = icon;
  }
  std::move(callback).Run(icon);
}

}  // namespace lynxtron
