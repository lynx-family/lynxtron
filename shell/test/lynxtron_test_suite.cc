// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "lynxtron/shell/test/lynxtron_test_suite.h"

#include "gin/array_buffer.h"
#include "gin/v8_initializer.h"
#include "v8/include/v8.h"

namespace lynxtron {

LynxtronTestSuite::LynxtronTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

LynxtronTestSuite::~LynxtronTestSuite() = default;

void LynxtronTestSuite::Initialize() {
  base::TestSuite::Initialize();

  // Initialize V8 for tests.
  v8::V8::SetFlagsFromString("--no-freeze-flags-after-init");

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
#endif

  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());
}

void LynxtronTestSuite::Shutdown() {
  base::TestSuite::Shutdown();
}

}  // namespace lynxtron
