// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/testbench_replay_controller.h"

#include <dispatch/dispatch.h>

#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "platform/embedder/lynx_recorder/test_bench_action_manager.h"
#include "platform/embedder/lynx_recorder/test_bench_replay_data_module.h"
#include "platform/embedder/public/capi/lynx_view_builder_capi.h"
#include "platform/embedder/public/lynx_template_data.h"
#include "platform/embedder/public/lynx_view.h"
#include "shell/api/lynx_view/testbench_record_fetcher.h"

namespace lynxtron {

namespace {

constexpr char kDefaultGlobalProps[] =
    R"({"theme":"light","platform":"macos"})";

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

void ScheduleReplayTask(std::function<void()> task, int64_t delay_millis) {
  auto* context = new std::function<void()>(std::move(task));
  dispatch_after_f(
      dispatch_time(DISPATCH_TIME_NOW, delay_millis * NSEC_PER_MSEC),
      dispatch_get_main_queue(), context, [](void* raw_context) {
        std::unique_ptr<std::function<void()>> replay_task(
            static_cast<std::function<void()>*>(raw_context));
        (*replay_task)();
      });
}

}  // namespace

bool IsTestbenchReplayUrl(std::string_view url) {
  return StartsWith(url, lynx::embedder::TEST_BENCH_URL_PREFIX);
}

bool IsLikelyTestbenchRecordFile(std::string_view data, bool is_json_file) {
  std::string_view trimmed = data;
  while (!trimmed.empty() &&
         std::isspace(static_cast<unsigned char>(trimmed.front()))) {
    trimmed.remove_prefix(1);
  }
  if (trimmed.empty()) {
    return false;
  }
  return (is_json_file && trimmed.front() == '{') ||
         LooksLikeBase64ZlibRecord(trimmed);
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
  Impl(lynx::pub::LynxView* view,
       std::function<void(double width, double height)> resize_callback) {
    auto view_ref =
        std::shared_ptr<lynx::pub::LynxView>(view, [](lynx::pub::LynxView*) {});
    action_manager_ = std::make_shared<lynx::embedder::TestBenchActionManager>(
        view_ref, std::move(resize_callback));
    action_manager_->SetFetchCallback(
        [](const std::string& request_url,
           std::function<void(const std::string& result)> callback) {
          FetchTestbenchRecord(request_url, std::move(callback));
        });
    action_manager_->SetTaskScheduler(ScheduleReplayTask);
  }

  void StartWithUrl(const std::string& url) {
    action_manager_->StartWithUrl(
        url,
        std::make_shared<lynx::pub::LynxTemplateData>(kDefaultGlobalProps));
  }

 private:
  std::shared_ptr<lynx::embedder::TestBenchActionManager> action_manager_;
};

TestbenchReplayController::TestbenchReplayController(
    lynx::pub::LynxView* view,
    std::function<void(double width, double height)> resize_callback)
    : impl_(std::make_unique<Impl>(view, std::move(resize_callback))) {}

TestbenchReplayController::~TestbenchReplayController() = default;

void TestbenchReplayController::StartWithUrl(const std::string& url) {
  impl_->StartWithUrl(url);
}

}  // namespace lynxtron
