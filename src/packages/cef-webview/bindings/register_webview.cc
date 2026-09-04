// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <lynx/registration.h>

extern "C" lynx_native_view_t* cef_webview_create_view(void* opaque);

LYNX_REGISTER_ELEMENT("CEFWebviewElementModule", "webview",
                      cef_webview_create_view, false, nullptr)
