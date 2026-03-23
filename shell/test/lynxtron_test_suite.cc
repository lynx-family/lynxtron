// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/test/lynxtron_test_suite.h"

#include "base/test/test_suite.h"

namespace lynxtron {

LynxtronTestSuite::LynxtronTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

LynxtronTestSuite::~LynxtronTestSuite() = default;

void LynxtronTestSuite::Initialize() {
  base::TestSuite::Initialize();
}

void LynxtronTestSuite::Shutdown() {
  base::TestSuite::Shutdown();
}

}  // namespace lynxtron
