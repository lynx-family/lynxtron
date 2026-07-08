// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_menu.h"

#include "shell/common/gin_helper/function_template.h"

namespace lynxtron::api {

Menu* Menu::New(gin::Arguments* args) {
  v8::Isolate* const isolate = args->isolate();
  Menu* const menu = new Menu(args);
  gin_helper::CallMethod(isolate, menu, "_init");
  return menu;
}

}  // namespace lynxtron::api
