// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/notifications/platform_notification_service.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "shell/api/notifications/notification_platform_bridge_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "shell/api/notifications/notification_platform_bridge_win.h"
#endif

namespace lynxtron {

// static
PlatformNotificationService* PlatformNotificationService::GetInstance() {
  static std::unique_ptr<PlatformNotificationService> instance;
  if (!instance) {
#if BUILDFLAG(IS_MAC)
    instance.reset(new NotificationPlatformBridgeMac());
#elif BUILDFLAG(IS_WIN)
    instance.reset(new NotificationPlatformBridgeWin());
#else
    // TODO: Implement for other platforms
#endif
  }
  return instance.get();
}

}  // namespace lynxtron
