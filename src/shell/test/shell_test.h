// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_TEST_SHELL_TEST_H_
#define LYNXTRON_SHELL_TEST_SHELL_TEST_H_

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lynxtron {

class ShellTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_TEST_SHELL_TEST_H_
