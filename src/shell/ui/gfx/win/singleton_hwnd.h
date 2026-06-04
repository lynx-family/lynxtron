// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_GFX_WIN_SINGLETON_HWND_H_
#define LYNXTRON_SHELL_UI_GFX_WIN_SINGLETON_HWND_H_

#include <windows.h>

#include "base/observer_list.h"
#include "shell/ui/gfx/win/window_impl.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace gfx {

class SingletonHwndObserver;

// Singleton message-only HWND that allows interested clients to receive WM_*
// notifications.
class SingletonHwnd : public WindowImpl {
 public:
  static SingletonHwnd* GetInstance();

  // Windows callback for WM_* notifications.
  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM wparam,
                            LPARAM lparam,
                            LRESULT& result,
                            DWORD msg_map_id) override;

 private:
  friend class SingletonHwndObserver;
  friend struct base::DefaultSingletonTraits<SingletonHwnd>;

  SingletonHwnd();
  ~SingletonHwnd() override;

  // Add/remove SingletonHwndObserver to forward WM_* notifications.
  void AddObserver(SingletonHwndObserver* observer);
  void RemoveObserver(SingletonHwndObserver* observer);

  // List of registered observers.
  base::ObserverList<SingletonHwndObserver, true>::Unchecked observer_list_;
};

}  // namespace gfx

#endif  // LYNXTRON_SHELL_UI_GFX_WIN_SINGLETON_HWND_H_
