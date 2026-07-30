// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/ui/gfx/win/hwnd_util.h"

#include <windows.h>

#include "base/debug/gdi_debug_util_win.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/win_util.h"

namespace gfx {

namespace {

// Don't inline these functions so they show up in crash reports.

NOINLINE void CrashAccessDenied(DWORD last_error) {
  LOG(FATAL) << last_error;
}

// Crash isn't one of the ones we commonly see.
NOINLINE void CrashOther(DWORD last_error) {
  LOG(FATAL) << last_error;
}

}  // namespace

std::wstring GetClassName(HWND window) {
  // GetClassNameW will return a truncated result (properly null terminated) if
  // the given buffer is not large enough.  So, it is not possible to determine
  // that we got the entire class name if the result is exactly equal to the
  // size of the buffer minus one.
  DWORD buffer_size = MAX_PATH;
  while (true) {
    std::wstring output;
    DWORD size_ret = GetClassNameW(
        window, base::WriteInto(&output, buffer_size), buffer_size);
    if (size_ret == 0) {
      break;
    }
    if (size_ret < (buffer_size - 1)) {
      output.resize(size_ret);
      return output;
    }
    buffer_size *= 2;
  }
  return std::wstring();  // error
}

#pragma warning(push)
#pragma warning(disable : 4312 4244)

WNDPROC SetWindowProc(HWND hwnd, WNDPROC proc) {
  // The reason we don't return the SetwindowLongPtr() value is that it returns
  // the orignal window procedure and not the current one. I don't know if it is
  // a bug or an intended feature.
  WNDPROC oldwindow_proc =
      reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwnd, GWLP_WNDPROC));
  SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(proc));
  return oldwindow_proc;
}

void* SetWindowUserData(HWND hwnd, void* user_data) {
  return reinterpret_cast<void*>(SetWindowLongPtr(
      hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(user_data)));
}

void* GetWindowUserData(HWND hwnd) {
  DWORD process_id = 0;
  GetWindowThreadProcessId(hwnd, &process_id);
  // A window outside the current process needs to be ignored.
  if (process_id != ::GetCurrentProcessId()) {
    return NULL;
  }
  return reinterpret_cast<void*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

#pragma warning(pop)

bool DoesWindowBelongToActiveWindow(HWND window) {
  DCHECK(window);
  HWND top_window = ::GetAncestor(window, GA_ROOT);
  if (!top_window) {
    return false;
  }

  HWND active_top_window = ::GetAncestor(::GetForegroundWindow(), GA_ROOT);
  return (top_window == active_top_window);
}

void CheckWindowCreated(HWND hwnd, DWORD last_error) {
  if (!hwnd) {
    switch (last_error) {
      case ERROR_NOT_ENOUGH_MEMORY:
        base::debug::CollectGDIUsageAndDie();
        break;
      case ERROR_ACCESS_DENIED:
        CrashAccessDenied(last_error);
        break;
      default:
        CrashOther(last_error);
        break;
    }
    LOG(FATAL) << last_error;
  }
}

extern "C" {
typedef HWND (*RootWindow)();
}

HWND GetWindowToParentTo(bool get_real_hwnd) {
  return get_real_hwnd ? ::GetDesktopWindow() : HWND_DESKTOP;
}

}  // namespace gfx
