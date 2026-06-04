// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "shell/test/lynxtron_test_suite.h"

int main(int argc, char** argv) {
  lynxtron::LynxtronTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&lynxtron::LynxtronTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
