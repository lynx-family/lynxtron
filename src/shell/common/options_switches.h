// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_OPTIONS_SWITCHES_H_
#define LYNXTRON_SHELL_COMMON_OPTIONS_SWITCHES_H_

#include <string_view>

#include "base/strings/cstring_view.h"

namespace lynxtron {

namespace options {

inline constexpr std::string_view kTitle = "title";
inline constexpr std::string_view kIcon = "icon";
inline constexpr std::string_view kFrame = "frame";
inline constexpr std::string_view kShow = "show";
inline constexpr std::string_view kCenter = "center";
inline constexpr std::string_view kX = "x";
inline constexpr std::string_view kY = "y";
inline constexpr std::string_view kWidth = "width";
inline constexpr std::string_view kHeight = "height";
inline constexpr std::string_view kUseContentSize = "useContentSize";
inline constexpr std::string_view kMinWidth = "minWidth";
inline constexpr std::string_view kMinHeight = "minHeight";
inline constexpr std::string_view kMaxWidth = "maxWidth";
inline constexpr std::string_view kMaxHeight = "maxHeight";
inline constexpr std::string_view kResizable = "resizable";
inline constexpr std::string_view kMovable = "movable";
inline constexpr std::string_view kMinimizable = "minimizable";
inline constexpr std::string_view kMaximizable = "maximizable";
inline constexpr std::string_view kFullScreenable = "fullscreenable";
inline constexpr std::string_view kClosable = "closable";

// whether to keep the window out of mission control
inline constexpr std::string_view kHiddenInMissionControl =
    "hiddenInMissionControl";

inline constexpr std::string_view kFullscreen = "fullscreen";

// Whether the window should show in taskbar.
inline constexpr std::string_view kSkipTaskbar = "skipTaskbar";

inline constexpr std::string_view kSimpleFullscreen = "simpleFullscreen";

// Make windows stays on the top of all other windows.
inline constexpr std::string_view kAlwaysOnTop = "alwaysOnTop";

// The requested title bar style for the window
inline constexpr std::string_view kTitleBarStyle = "titleBarStyle";

// Tabbing identifier for the window if native tabs are enabled on macOS.
inline constexpr std::string_view kTabbingIdentifier = "tabbingIdentifier";

// Whether the window should be transparent.
inline constexpr std::string_view kTransparent = "transparent";

// Window type hint.
inline constexpr std::string_view kType = "type";

// Disable auto-hiding cursor.
inline constexpr std::string_view kDisableAutoHideCursor =
    "disableAutoHideCursor";

// Whether the window should have a shadow.
inline constexpr std::string_view kHasShadow = "hasShadow";

// Browser window opacity
inline constexpr std::string_view kOpacity = "opacity";

// The window's background color, supports CSS-like color strings.
inline constexpr std::string_view kBackgroundColor = "backgroundColor";

// Whether the window can be activated.
inline constexpr std::string_view kFocusable = "focusable";

// Add a vibrancy effect to the browser window
inline constexpr std::string_view kVibrancyType = "vibrancy";
inline constexpr std::string_view kVisualEffectState = "visualEffectState";

inline constexpr std::string_view kTrafficLightPosition =
    "trafficLightPosition";
inline constexpr std::string_view kRoundedCorners = "roundedCorners";

// Enable the node integration.
inline constexpr std::string_view kNodeIntegration = "nodeIntegration";

}  // namespace options

// Following are actually command line switches, should be moved to other files.

namespace switches {

// Specifies the flags passed to JS engine.
inline constexpr base::cstring_view kJavaScriptFlags = "js-flags";

}  // namespace switches

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_OPTIONS_SWITCHES_H_
