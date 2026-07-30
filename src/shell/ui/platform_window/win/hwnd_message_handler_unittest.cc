// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/ui/platform_window/win/hwnd_message_handler.h"

#include <limits.h>

#include <ostream>

#include "shell/ui/gfx/geometry/size.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

// This test target does not depend on Chromium's full geometry test-support
// library. Keep the formatter required by gtest's Size assertions local to the
// test executable.
void PrintTo(const Size& size, ::std::ostream* os) {
  *os << size.ToString();
}

}  // namespace gfx

namespace ui {

TEST(HWNDMessageHandlerTest, ExpandMaxTrackSizePreservesUnboundedDimensions) {
  const gfx::Size non_client_size(16, 39);

  EXPECT_EQ(gfx::Size(0, 539), ExpandMaxTrackSizeForClientFrame(
                                   gfx::Size(0, 500), non_client_size));
  EXPECT_EQ(gfx::Size(416, 0), ExpandMaxTrackSizeForClientFrame(
                                   gfx::Size(400, 0), non_client_size));
  EXPECT_EQ(gfx::Size(INT_MAX, 539),
            ExpandMaxTrackSizeForClientFrame(gfx::Size(INT_MAX, 500),
                                             non_client_size));
  EXPECT_EQ(gfx::Size(416, INT_MAX),
            ExpandMaxTrackSizeForClientFrame(gfx::Size(400, INT_MAX),
                                             non_client_size));
}

}  // namespace ui
