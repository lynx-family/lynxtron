// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_TESTBENCH_REPLAY_CONTROLLER_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_TESTBENCH_REPLAY_CONTROLLER_H_

#include <memory>
#include <string>
#include <string_view>

struct lynx_view_builder_t;

namespace lynx {
namespace pub {
class LynxView;
}  // namespace pub
}  // namespace lynx

namespace lynxtron {

bool IsTestbenchReplayUrl(std::string_view url);
bool IsLikelyTestbenchRecordFile(std::string_view data, bool is_json_file);
std::string MakeTestbenchReplayUrl(const std::string& record_url);
void RegisterTestbenchReplayDataModule(lynx_view_builder_t* builder);

class TestbenchReplayController {
 public:
  explicit TestbenchReplayController(lynx::pub::LynxView* view);
  ~TestbenchReplayController();

  void StartWithUrl(const std::string& url);

  TestbenchReplayController(const TestbenchReplayController&) = delete;
  TestbenchReplayController& operator=(const TestbenchReplayController&) =
      delete;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_TESTBENCH_REPLAY_CONTROLLER_H_
