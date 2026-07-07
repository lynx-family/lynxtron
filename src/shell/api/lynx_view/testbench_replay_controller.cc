// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/testbench_replay_controller.h"

#include <cctype>
#include <memory>
#include <string>
#include <string_view>

#include "platform/embedder/lynx_recorder/test_bench_action_manager.h"
#include "platform/embedder/lynx_recorder/test_bench_replay_data_module.h"
#include "platform/embedder/lynx_recorder/test_bench_utils.h"
#include "platform/embedder/public/capi/lynx_view_builder_capi.h"
#include "platform/embedder/public/lynx_view.h"
#include "shell/api/lynx_view/testbench_record_fetcher.h"

namespace lynxtron {

namespace {

bool StartsWith(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.substr(0, prefix.size()) == prefix;
}

bool IsBase64Byte(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
}

bool LooksLikeBase64ZlibRecord(std::string_view data) {
  if (!StartsWith(data, "eJ")) {
    return false;
  }

  bool has_payload = false;
  size_t checked = 0;
  for (char c : data) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      continue;
    }
    if (!IsBase64Byte(c)) {
      return false;
    }
    has_payload = true;
    if (++checked >= 256) {
      break;
    }
  }
  return has_payload;
}

}  // namespace

bool IsTestbenchReplayUrl(std::string_view url) {
  return StartsWith(url, lynx::embedder::TEST_BENCH_URL_PREFIX);
}

bool IsLikelyTestbenchRecordFile(std::string_view data, bool is_json_file) {
  (void)is_json_file;
  std::string_view trimmed = data;
  while (!trimmed.empty() &&
         std::isspace(static_cast<unsigned char>(trimmed.front()))) {
    trimmed.remove_prefix(1);
  }
  if (trimmed.empty()) {
    return false;
  }
  if (trimmed.front() == '{') {
    return trimmed.find("\"Action List\"") != std::string_view::npos;
  }
  return LooksLikeBase64ZlibRecord(trimmed);
}

std::string MakeTestbenchReplayUrl(const std::string& record_url) {
  return lynx::embedder::TEST_BENCH_URL_PREFIX + "url=" + record_url;
}

void RegisterTestbenchReplayDataModule(lynx_view_builder_t* builder) {
  lynx::embedder::TestBenchReplayDataModule::RegisterJSB(
      [builder](const std::string& name, napi_module_creator creator) {
        lynx_view_builder_register_native_module(builder, name.c_str(), creator,
                                                 nullptr);
      });
}

class TestbenchReplayController::Impl {
 public:
  explicit Impl(lynx::pub::LynxView* view) {
    auto view_ref =
        std::shared_ptr<lynx::pub::LynxView>(view, [](lynx::pub::LynxView*) {});
    action_manager_ = std::make_shared<lynx::embedder::TestBenchActionManager>(
        view_ref, [](int, int) {});
    action_manager_->SetFetchCallback(
        [](const std::string& request_url,
           std::function<void(const std::string& result)> callback) {
          auto result = FetchTestbenchRecord(request_url);
          callback(result.value_or(std::string()));
        });
  }

  void StartWithUrl(const std::string& url) {
    action_manager_->StartWithUrl(url);
  }

 private:
  std::shared_ptr<lynx::embedder::TestBenchActionManager> action_manager_;
};

TestbenchReplayController::TestbenchReplayController(lynx::pub::LynxView* view)
    : impl_(std::make_unique<Impl>(view)) {}

TestbenchReplayController::~TestbenchReplayController() = default;

void TestbenchReplayController::StartWithUrl(const std::string& url) {
  impl_->StartWithUrl(url);
}

}  // namespace lynxtron
