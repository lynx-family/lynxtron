// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_APP_WIN_REG_HELPER_H_
#define LYNXTRON_SHELL_APP_WIN_REG_HELPER_H_

#include <string>

namespace lynxtron {

class RegHelper {
 public:
  static bool GetDeviceManufacturer(std::string& manufacture);
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_WIN_REG_HELPER_H_
