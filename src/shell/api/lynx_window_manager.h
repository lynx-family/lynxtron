// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_WINDOW_MANAGER_H_
#define LYNXTRON_SHELL_API_LYNX_WINDOW_MANAGER_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "shell/api/api_lynx_window.h"

namespace lynxtron {
namespace api {

struct LynxWindowWeakPtrCompare {
  bool operator()(const base::WeakPtr<LynxWindow>& a,
                  const base::WeakPtr<LynxWindow>& b) const {
    return a.get() < b.get();
  }
};

class LynxWindowManager {
 public:
  static LynxWindowManager* GetInstance();

  LynxWindowManager();
  ~LynxWindowManager();

  // disable copy
  LynxWindowManager(const LynxWindowManager&) = delete;
  LynxWindowManager& operator=(const LynxWindowManager&) = delete;

  void RegisterLynxWindow(base::WeakPtr<LynxWindow> lynx_window);
  void UnregisterLynxWindow(base::WeakPtr<LynxWindow> lynx_window);

  const std::set<base::WeakPtr<LynxWindow>, LynxWindowWeakPtrCompare>&
  GetLynxWindows() const {
    return lynx_window_set_;
  }

 private:
  std::set<base::WeakPtr<LynxWindow>, LynxWindowWeakPtrCompare>
      lynx_window_set_;
};

}  // namespace api
}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_LYNX_WINDOW_MANAGER_H_
