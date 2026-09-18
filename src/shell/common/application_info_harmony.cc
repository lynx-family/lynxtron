// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <string>

#include "shell/common/application_info.h"

namespace lynxtron {

// HarmonyOS fallbacks for the three GetApplication* free functions
// declared cross-platform in application_info.h:28-30 but defined only in
// application_info_mac.mm and application_info_win.cc.
//
// GetApplicationName falls back to a non-empty literal because the
// cross-platform GetPossiblyOverriddenApplicationName (application_info.cc:31)
// uses an empty-string check to decide between the override and the
// platform-provided name; returning "" here would silently swap behaviour.
//
// A native implementation can read bundle metadata through the OHOS bundle
// manager API.
//
// NOT stubbed here:
//   - GetRawAppUserModelID / GetAppUserModelID / SetAppUserModelID /
//     IsRunningInDesktopBridge (application_info.h:36-40) are inside
//     `#if BUILDFLAG(IS_WIN)` and thus not declared on harmony.

// std::string GetApplicationName() {
//   return "Lynxtron";
// }

// std::string GetApplicationVersion() {
//   return std::string();
// }

std::string GetApplicationName() {
  // attempt #1: the string set in app.setName()
  std::string ret = OverriddenApplicationName();

  // attempt #2: Electron's name
  if (ret.empty()) {
    ret = "Lynxtron";
  }
  return ret;
}

std::string GetApplicationVersion() {
  std::string ret;
  std::string ELECTRON_PRODUCT_NAME = "Lynxtron";
  std::string ELECTRON_VERSION_STRING = "0.0.1";

  // ensure ELECTRON_PRODUCT_NAME and GetApplicationVersion match up
  if (GetApplicationName() == ELECTRON_PRODUCT_NAME) {
    ret = ELECTRON_VERSION_STRING;
  }

  // try to use the string set in app.setVersion()
  if (ret.empty()) {
    ret = OverriddenApplicationVersion();
  }

  // no name and version fields in package.json
  if (ret.empty()) {
    ret = ELECTRON_VERSION_STRING;
  }
  return ret;
}

std::string GetApplicationId() {
  return std::string();
}

}  // namespace lynxtron
