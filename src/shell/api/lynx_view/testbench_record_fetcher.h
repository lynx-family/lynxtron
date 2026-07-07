// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_TESTBENCH_RECORD_FETCHER_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_TESTBENCH_RECORD_FETCHER_H_

#include <optional>
#include <string>

namespace lynxtron {

std::optional<std::string> FetchTestbenchRecord(const std::string& url);

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_TESTBENCH_RECORD_FETCHER_H_
