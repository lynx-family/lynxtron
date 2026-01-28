// Copyright (c) 2022 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_COMMON_THREAD_RESTRICTIONS_H_
#define LYNXTRON_SHELL_COMMON_THREAD_RESTRICTIONS_H_

#include "base/threading/thread_restrictions.h"

namespace lynxtron {

class ScopedAllowBlockingForLynxtron : public base::ScopedAllowBlocking {};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_THREAD_RESTRICTIONS_H_
