// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_BASE_WIN_SHELL_H_
#define LYNXTRON_SHELL_UI_BASE_WIN_SHELL_H_

#include <string>

#include "base/component_export.h"
#include "base/win/windows_types.h"

namespace base {
class FilePath;
}

namespace ui::win {

// Open the folder at |full_path| via the Windows shell. It is an error if
// |full_path| does not refer to a folder.
//
// Note: Must be called on a thread that allows blocking.
COMPONENT_EXPORT(UI_BASE)
bool OpenFolderViaShell(const base::FilePath& full_path);

// Invokes the default verb on the file specified by |full_path| via the Windows
// shell. Usually, the default verb is "open" unless specified otherwise for the
// file type.
//
// In the event that there is no default application registered for the
// specified file, asks the user via the Windows "Open With" dialog.  Returns
// |true| on success.
//
// Note: Must be called on a thread that allows blocking.
COMPONENT_EXPORT(UI_BASE)
bool OpenFileViaShell(const base::FilePath& full_path);

// Disables the ability of the specified window to be pinned to the taskbar or
// the Start menu. This will remove "Pin this program to taskbar" from the
// taskbar menu of the specified window.
COMPONENT_EXPORT(UI_BASE) bool PreventWindowFromPinning(HWND hwnd);

// Clears the Window Property Store on an HWND.
COMPONENT_EXPORT(UI_BASE) void ClearWindowPropertyStore(HWND hwnd);

// Returns true if dwm composition is available and turned on on the current
// platform.
// This method supports a command-line override for testing.
COMPONENT_EXPORT(UI_BASE) bool IsAeroGlassEnabled();

// Returns true if dwm composition is available and turned on on the current
// platform.
COMPONENT_EXPORT(UI_BASE) bool IsDwmCompositionEnabled();

}  // namespace ui::win

#endif  // LYNXTRON_SHELL_UI_BASE_WIN_SHELL_H_
