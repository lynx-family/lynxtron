// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/lynxtron_config.h"

namespace lynxtron {

LynxtronConfig& LynxtronConfig::GetInstance() {
  static LynxtronConfig instance;
  return instance;
}

void LynxtronConfig::SetWebView2FixedRuntimePath(const std::string& path) {
  webview2_fixed_runtime_path_ = path;
}

const std::string& LynxtronConfig::GetWebView2FixedRuntimePath() const {
  return webview2_fixed_runtime_path_;
}

}  // namespace lynxtron
