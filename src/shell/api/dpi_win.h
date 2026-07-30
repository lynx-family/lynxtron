// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_API_DPI_WIN_H_
#define LYNXTRON_SHELL_API_DPI_WIN_H_

#include <windows.h>

#include <optional>

#include "shell/ui/gfx/geometry/point.h"
#include "shell/ui/gfx/geometry/rect.h"
#include "shell/ui/gfx/geometry/size.h"

namespace lynxtron {

// Windows window-coordinate conversion policy
// --------------------------------------------
// NativeWindow exposes window and content geometry in DIP, while HWND and the
// Win32 window-management APIs use physical screen pixels. Keep the conversion
// boundary in this file so callers do not accidentally mix those units.
//
// Origins and sizes intentionally follow different rules:
//
//  * ScreenWin owns display selection and converts screen-space origins using
//    Chromium's monitor-relative, rounded point conversion.
//  * Window sizes use gfx::ScaleToRoundedSize independently of the origin.
//    Minimum size, maximum size, and the actual window size must all use this
//    same rule. Do not replace it with enclosing/ceiled rect conversion:
//    scaling both rect edges can make the resulting size depend on the origin,
//    so moving a non-resizable window could change its reported DIP size.
//  * INT_MAX is the Windows-side "unbounded" maximum-size sentinel and remains
//    INT_MAX instead of being scaled.
//
// Display selection is part of the API contract:
//
//  * Pass a non-null HWND when converting geometry belonging to the window on
//    its current display, for example its current client size.
//  * Pass nullptr for target, restored, or moving screen rects. ScreenWin then
//    selects the display containing the supplied rect instead of using the
//    window's old display. This is required when a window crosses displays with
//    different scale factors.
//  * A size alone has no screen position. Its HWND selects the scale factor;
//    nullptr uses ScreenWin's default HWND/display fallback and is appropriate
//    only before a real window/display context exists.
//
// Integer DIP and pixel grids do not have a universally reversible mapping at
// fractional scale factors. These helpers provide stable window API semantics,
// not a guarantee that every arbitrary pixel value round-trips unchanged.

// Converts an outer or client screen rect from physical pixels to DIP. The
// origin uses ScreenWin's rounded point mapping; the size is rounded
// independently using the selected display's scale factor.
gfx::Rect ScreenToDIPRectForWindow(HWND hwnd, const gfx::Rect& pixel_bounds);

// Converts an outer or client screen rect from DIP to physical pixels. The
// origin uses ScreenWin's rounded point mapping; the size is scaled
// independently using the selected display's scale factor, so it does not
// depend on edge rounding. Moving the target to another display may still
// change its pixel size according to that display's DPI.
gfx::Rect DIPToScreenRectForWindow(HWND hwnd, const gfx::Rect& dip_bounds);

// Converts a window/client size from DIP to physical pixels using the HWND's
// display scale and the same rounded-size rule used by the rect helpers. A null
// HWND uses ScreenWin's default display fallback.
gfx::Size DIPToScreenSizeForWindow(HWND hwnd, const gfx::Size& dip_size);

// Converts a window/client size from physical pixels to DIP using the HWND's
// display scale and the same rounded-size rule used by the rect helpers. A null
// HWND uses ScreenWin's default display fallback.
gfx::Size ScreenToDIPSizeForWindow(HWND hwnd, const gfx::Size& pixel_size);

// Updates a physical window rect from optional DIP origin and size values.
// Fields that are not supplied retain their exact physical pixel values:
//
//  * Supplying both fields converts the complete target rect and selects the
//    display from that target.
//  * Supplying only the origin converts that target point and selects its
//    display while preserving the exact physical pixel size.
//  * Supplying only the size uses the display containing |pixel_bounds|.
//
// This prevents position-only and size-only operations from round-tripping the
// untouched physical field through the integer DIP grid.
gfx::Rect UpdateScreenRectForWindow(const gfx::Rect& pixel_bounds,
                                    const std::optional<gfx::Point>& dip_origin,
                                    const std::optional<gfx::Size>& dip_size);
}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_DPI_WIN_H_
