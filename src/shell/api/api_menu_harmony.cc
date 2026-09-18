// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_menu.h"

namespace lynxtron::api {

// HarmonyOS fallback for Menu::New, which is declared in api_menu.h:37
// without a #if guard but defined only in api_menu.cc:402 inside an
// `#if BUILDFLAG(IS_WIN)` block, plus api_menu_mac.mm. Returns nullptr so
// any JS caller that constructs a Menu sees a creation failure rather
// than a crash. Native menu integration depends on OHOS UI container APIs.
Menu* Menu::New(gin::Arguments* args) {
  return nullptr;
}

}  // namespace lynxtron::api
