// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "tray_icon.h"

#include <memory>

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "tray_icon_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "tray_icon_win.h"
#endif

namespace lynxtron {

std::unique_ptr<TrayIcon> TrayIcon::Create(TrayIconObserver* observer,
                                           const std::string& guid) {
#if BUILDFLAG(IS_MAC)
  return std::make_unique<TrayIconMac>(observer, guid);
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<TrayIconWin>(observer, guid);
#else
  return nullptr;
#endif
}

}  // namespace lynxtron
