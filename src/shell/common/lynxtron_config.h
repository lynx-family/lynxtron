// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_LYNXTRON_CONFIG_H_
#define LYNXTRON_SHELL_COMMON_LYNXTRON_CONFIG_H_

#include <string>

namespace lynxtron {

class LynxtronConfig {
 public:
  static LynxtronConfig& GetInstance();

  void SetWebView2FixedRuntimePath(const std::string& path);
  const std::string& GetWebView2FixedRuntimePath() const;

 private:
  LynxtronConfig() = default;
  std::string webview2_fixed_runtime_path_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_LYNXTRON_CONFIG_H_
