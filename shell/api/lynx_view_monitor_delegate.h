// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SHELL_API_LYNX_VIEW_MONITOR_DELEGATE_H_
#define SHELL_API_LYNX_VIEW_MONITOR_DELEGATE_H_

#include <cstdint>
#include <string_view>

#include "lynxtron/shell/common/global_delegate_registry.h"

namespace lynxtron {

static const char* kLynxViewMonitorDelegateName = "LynxViewMonitorDelegate";

class LynxViewMonitorDelegate : public GlobalDelegate {
 public:
  virtual ~LynxViewMonitorDelegate() = default;
  void SetInstanceId(int64_t instance_id) { instance_id_ = instance_id; }

  virtual void OnPageStart(std::string_view url) = 0;
  virtual void OnRuntimeReady() = 0;
  virtual void OnReceivedError(int error_code, std::string_view message) = 0;
  virtual void OnTimingSetup(std::string_view timing_info) = 0;
  virtual void OnTimingUpdate(std::string_view timing_info,
                              std::string_view update_timing,
                              std::string_view update_flag) = 0;
  virtual void OnDestroy() = 0;

 protected:
  int64_t instance_id_ = 0;
};

}  // namespace lynxtron

#endif  // SHELL_API_LYNX_VIEW_MONITOR_DELEGATE_H_
