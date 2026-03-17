// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_GFX_WIN_SINGLETON_HWND_OBSERVER_H_
#define LYNXTRON_SHELL_UI_GFX_WIN_SINGLETON_HWND_OBSERVER_H_

#include <windows.h>

#include "base/functional/callback.h"

namespace gfx {

class SingletonHwnd;

// Singleton lifetime management is tricky. This observer handles the correct
// cleanup if either the SingletonHwnd or forwarded object is destroyed first.
// Note that if you want to register a hot key on the SingletonHwnd, you need to
// use a SingletonHwndHotKeyObserver instead for each hot key.
class SingletonHwndObserver {
 public:
  using WndProc = base::RepeatingCallback<void(HWND, UINT, WPARAM, LPARAM)>;

  explicit SingletonHwndObserver(const WndProc& wnd_proc);
  ~SingletonHwndObserver();

 private:
  friend class SingletonHwnd;

  void ClearWndProc();
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  WndProc wnd_proc_;
};

}  // namespace gfx

#endif  // LYNXTRON_SHELL_UI_GFX_WIN_SINGLETON_HWND_OBSERVER_H_
