// Copyright 2021 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_CLIENT_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_CLIENT_H_

#include <string_view>

namespace lynxtron {
class LynxViewClient {
 public:
  virtual ~LynxViewClient() = default;

  virtual void OnPageStart(std::string_view url) = 0;
  virtual void OnLoadSuccess() = 0;
  virtual void OnFirstScreen() = 0;
  virtual void OnPageUpdated() = 0;
  virtual void OnDataUpdated() = 0;
  virtual void OnDestroy() = 0;
  virtual void OnRuntimeReady() = 0;
  virtual void OnReceivedError(int error_code, std::string_view message) = 0;
  virtual void OnTimingSetup(std::string_view timing_info) = 0;
  virtual void OnTimingUpdate(std::string_view timing_info,
                              std::string_view update_timing,
                              std::string_view update_flag) = 0;
  virtual void OnEnterForeground() = 0;
  virtual void OnEnterBackground() = 0;

  virtual void OnFrameTiming(int64_t frame_start_time_in_ns,
                             int64_t frame_finish_time_in_ns) = 0;
};

}  // namespace lynxtron
#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_VIEW_CLIENT_H_
