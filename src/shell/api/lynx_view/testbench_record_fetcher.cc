// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/testbench_record_fetcher.h"

#include "build/build_config.h"

#if !BUILDFLAG(IS_MAC)
namespace lynxtron {

std::optional<std::string> FetchTestbenchRecord(const std::string& url) {
  return std::nullopt;
}

}  // namespace lynxtron
#endif
