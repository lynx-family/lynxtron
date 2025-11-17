// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "lynxtron/shell/test/lynxtron_test_suite.h"

int main(int argc, char** argv) {
  lynxtron::LynxtronTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&lynxtron::LynxtronTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
