// Copyright (c) 2026 Lynxtron Authors.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_UI_ACCELERATOR_UTIL_H_
#define LYNXTRON_SHELL_UI_ACCELERATOR_UTIL_H_

#include <string>

#include "shell/ui/accelerator.h"

namespace accelerator_util {

bool StringToAccelerator(const std::string& accelerator, ui::Accelerator* out);

}  // namespace accelerator_util

#endif  // LYNXTRON_SHELL_UI_ACCELERATOR_UTIL_H_
