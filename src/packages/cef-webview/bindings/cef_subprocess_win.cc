// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <windows.h>

#include "include/cef_app.h"

extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

extern "C" int cef_webview_execute_process(HINSTANCE instance);

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  return cef_webview_execute_process(instance);
}
