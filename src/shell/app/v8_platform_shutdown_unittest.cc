// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstdlib>

#include "core/runtime/js/jsi/v8/v8_platform_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lynxtron {

TEST(V8PlatformShutdownTest, WaitsForEveryIsolateAndRejectsLateWork) {
  // Shutdown is terminal; isolate it from other tests and repeated runs.
  ASSERT_EXIT(
      {
        using lynx::runtime::js::V8PlatformData;
        ASSERT_TRUE(V8PlatformData::TryRegisterIsolate());
        ASSERT_TRUE(V8PlatformData::TryRegisterIsolate());
        ASSERT_TRUE(V8PlatformData::TryRegisterIsolate());
        // Include an isolate that finished before terminal shutdown was
        // requested.
        V8PlatformData::IsolateFinished();

        int completions = 0;
        V8PlatformData::Shutdown([&completions] {
          ++completions;
          // Completion must run outside the registry lock and keep work sealed.
          EXPECT_FALSE(V8PlatformData::TryRegisterIsolate());
        });
        EXPECT_EQ(completions, 0);
        EXPECT_FALSE(V8PlatformData::TryRegisterIsolate());

        V8PlatformData::IsolateFinished();
        EXPECT_EQ(completions, 0);
        V8PlatformData::IsolateFinished();
        EXPECT_EQ(completions, 1);
        EXPECT_FALSE(V8PlatformData::TryRegisterIsolate());
        std::_Exit(testing::Test::HasFailure() ? 1 : 0);
      },
      testing::ExitedWithCode(0), "");
}

}  // namespace lynxtron
