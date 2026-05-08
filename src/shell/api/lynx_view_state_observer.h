// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_STATE_OBSERVER_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_STATE_OBSERVER_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "src/shell/common/global_delegate_registry.h"

namespace lynxtron {

class LynxViewBuilder;

static const char* kLynxViewStateObserverName = "LynxViewStateObserver";

class LynxViewStateObserver : public GlobalDelegate {
 public:
  virtual ~LynxViewStateObserver() = default;
  void SetInstanceId(int64_t instance_id) { instance_id_ = instance_id; }
  virtual void ReportJSError(const std::string& error_info) = 0;
  virtual void ConfigJSBase(const std::string& js_base) = 0;
  virtual void CustomReport(const std::string& custom_data) = 0;

  virtual void OnPreLynxViewCreate(LynxViewBuilder* builder) = 0;
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

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_STATE_OBSERVER_H_
