// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_APP_WINDOW_LIST_H_
#define LYNXTRON_SHELL_APP_WINDOW_LIST_H_

#include <vector>

#include "base/observer_list.h"

namespace lynxtron {

class NativeWindow;
class WindowListObserver;

class WindowList {
 public:
  // disable copy
  WindowList(const WindowList&) = delete;
  WindowList& operator=(const WindowList&) = delete;

  typedef std::vector<NativeWindow*> WindowVector;

  static WindowVector GetWindows();
  static bool IsEmpty();

  // Adds or removes |window| from the list it is associated with.
  static void AddWindow(NativeWindow* window);
  static void RemoveWindow(NativeWindow* window);

  // Called by window when a close is cancelled by beforeunload handler.
  static void WindowCloseCancelled(NativeWindow* window);

  // Adds and removes |observer| from the observer list.
  static void AddObserver(WindowListObserver* observer);
  static void RemoveObserver(WindowListObserver* observer);

  // Closes all windows.
  static void CloseAllWindows();

  // Destroy all windows.
  static void DestroyAllWindows();

 private:
  static WindowList* GetInstance();

  WindowList();
  ~WindowList();

  // A list of observers which will be notified of every window addition and
  // removal across all WindowLists.
  [[nodiscard]] static base::ObserverList<WindowListObserver>& GetObservers();

  // A vector of the windows in this list, in the order they were added.
  WindowVector windows_;

  static WindowList* instance_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_WINDOW_LIST_H_
