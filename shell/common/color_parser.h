// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_COLOR_PARSER_H_
#define LYNXTRON_SHELL_COMMON_COLOR_PARSER_H_

#include <string>

typedef unsigned int SkColor;

namespace content {

// Parses a CSS-style color string from hex (3- or 6-digit), rgb(), rgba(),
// hsl() or hsla() formats. Returns true on success.
bool ParseCssColorString(const std::string& color_string, SkColor* result);

// Parses a RGB or RGBA string like #FF9982CC, #FF9982, #EEEE, or #EEE to a
// color. Returns true for success.
bool ParseHexColorString(const std::string& color_string, SkColor* result);

// Parses rgb() or rgba() string to a color. Returns true for success.
bool ParseRgbColorString(const std::string& color_string, SkColor* result);

}  // namespace content

#endif  // LYNXTRON_SHELL_COMMON_COLOR_PARSER_H_
