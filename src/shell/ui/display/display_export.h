// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_DISPLAY_DISPLAY_EXPORT_H_
#define LYNXTRON_SHELL_UI_DISPLAY_DISPLAY_EXPORT_H_

// Defines DISPLAY_EXPORT so that functionality implemented by the UI module
// can be exported to consumers.

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(DISPLAY_IMPLEMENTATION)
#define DISPLAY_EXPORT __declspec(dllexport)
#else
#define DISPLAY_EXPORT __declspec(dllimport)
#endif

#else  // !defined(WIN32)

#if defined(DISPLAY_IMPLEMENTATION)
#define DISPLAY_EXPORT __attribute__((visibility("default")))
#else
#define DISPLAY_EXPORT
#endif

#endif

#else  // !defined(COMPONENT_BUILD)

#define DISPLAY_EXPORT

#endif

#endif  // LYNXTRON_SHELL_UI_DISPLAY_DISPLAY_EXPORT_H_
