// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/headless_windowless_renderer.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "lynx/platform/embedder/public/capi/lynx_windowless_renderer_capi.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

namespace lynxtron {

namespace {

scoped_refptr<base::SingleThreadTaskRunner>& GlobalWindowlessTaskRunner() {
  static base::NoDestructor<scoped_refptr<base::SingleThreadTaskRunner>>
      task_runner;
  return *task_runner;
}

std::atomic<bool>& GlobalWindowlessTaskRunnerInstallAttempted() {
  static std::atomic<bool> attempted(false);
  return attempted;
}

std::atomic<bool>& GlobalWindowlessTaskRunnerInstalled() {
  static std::atomic<bool> installed(false);
  return installed;
}

std::atomic<uint64_t>& GlobalWindowlessTasksPosted() {
  static std::atomic<uint64_t> posted(0);
  return posted;
}

std::atomic<uint64_t>& GlobalWindowlessTasksRun() {
  static std::atomic<uint64_t> run(0);
  return run;
}

std::atomic<uint64_t>& GlobalWindowlessTasksFailed() {
  static std::atomic<uint64_t> failed(0);
  return failed;
}

base::TimeDelta DelayFromTargetTime(uint64_t target_time_nanos) {
  const uint64_t now_nanos = static_cast<uint64_t>(
      base::Time::Now().InMillisecondsSinceUnixEpoch()) *
      1000ULL * 1000ULL;
  if (target_time_nanos <= now_nanos) {
    return base::TimeDelta();
  }
  return base::Nanoseconds(target_time_nanos - now_nanos);
}

void RunGlobalWindowlessTask(lynx_task_t task) {
  ++GlobalWindowlessTasksRun();
  if (!lynx_windowless_run_ui_task(task)) {
    ++GlobalWindowlessTasksFailed();
  }
}

void InstallGlobalWindowlessTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!task_runner || GlobalWindowlessTaskRunner()) {
    return;
  }

  GlobalWindowlessTaskRunner() = std::move(task_runner);
  lynx_windowless_ui_task_runner_config_t config{};
  config.struct_size = sizeof(config);
  config.runs_on_current_thread_callback = [](void*) -> bool {
    auto runner = GlobalWindowlessTaskRunner();
    return runner && runner->BelongsToCurrentThread();
  };
  config.post_task_callback = [](lynx_task_t task, uint64_t target_time_nanos,
                                 void*) {
    auto runner = GlobalWindowlessTaskRunner();
    if (!runner) {
      return;
    }
    ++GlobalWindowlessTasksPosted();
    runner->PostDelayedTask(FROM_HERE,
                            base::BindOnce(&RunGlobalWindowlessTask, task),
                            DelayFromTargetTime(target_time_nanos));
  };
  GlobalWindowlessTaskRunnerInstallAttempted() = true;
  GlobalWindowlessTaskRunnerInstalled() =
      lynx_windowless_set_global_ui_task_runner(&config);
}

}  // namespace

HeadlessWindowlessRenderer::HeadlessWindowlessRenderer()
    : LynxWindowlessRenderer(kRendererTypeSoftware),
      task_runner_(base::SingleThreadTaskRunner::HasCurrentDefault()
                       ? base::SingleThreadTaskRunner::GetCurrentDefault()
                       : nullptr) {
  InstallGlobalWindowlessTaskRunner(task_runner_);
}

HeadlessWindowlessRenderer::~HeadlessWindowlessRenderer() = default;

bool HeadlessWindowlessRenderer::OnSoftwarePresent(const void* allocation,
                                                   size_t row_bytes,
                                                   size_t height) {
  if (!allocation || row_bytes == 0 || height == 0) {
    return false;
  }

  const auto* bytes = static_cast<const uint8_t*>(allocation);
  const size_t byte_count = row_bytes * height;
  {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    latest_frame_.assign(bytes, bytes + byte_count);
    latest_row_bytes_ = row_bytes;
    latest_height_ = height;
    ++frames_presented_;
  }
  return true;
}

void HeadlessWindowlessRenderer::OnPostTask(lynx_task_t task,
                                            uint64_t interval_nanoseconds) {
  ++tasks_posted_;
  auto renderer = std::static_pointer_cast<HeadlessWindowlessRenderer>(
      shared_from_this());
  if (!task_runner_) {
    ++tasks_run_;
    RunTask(task);
    return;
  }
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](std::shared_ptr<HeadlessWindowlessRenderer> renderer,
             lynx_task_t task) {
            ++renderer->tasks_run_;
            renderer->RunTask(task);
          },
          std::move(renderer), task),
      base::Nanoseconds(interval_nanoseconds));
}

bool HeadlessWindowlessRenderer::CopyLastFrameToPng(
    std::vector<uint8_t>* output) const {
  if (!output) {
    return false;
  }

  std::vector<uint8_t> frame;
  size_t row_bytes = 0;
  size_t height = 0;
  {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    frame = latest_frame_;
    row_bytes = latest_row_bytes_;
    height = latest_height_;
  }

  if (frame.empty() || row_bytes == 0 || height == 0 ||
      row_bytes % 4 != 0) {
    return false;
  }

  const int width = static_cast<int>(row_bytes / 4);
  const int frame_height = static_cast<int>(height);
  return gfx::PNGCodec::Encode(frame.data(), gfx::PNGCodec::FORMAT_BGRA,
                               gfx::Size(width, frame_height),
                               static_cast<int>(row_bytes), false, {}, output);
}

bool HeadlessWindowlessRenderer::DispatchTextInput(const std::string& text) {
  return SendTextInput(text.c_str());
}

uint64_t HeadlessWindowlessRenderer::frames_presented() const {
  std::lock_guard<std::mutex> lock(frame_mutex_);
  return frames_presented_;
}

uint64_t HeadlessWindowlessRenderer::tasks_posted() const {
  return tasks_posted_.load();
}

uint64_t HeadlessWindowlessRenderer::tasks_run() const {
  return tasks_run_.load();
}

bool HeadlessWindowlessRenderer::global_ui_task_runner_install_attempted() {
  return GlobalWindowlessTaskRunnerInstallAttempted().load();
}

bool HeadlessWindowlessRenderer::global_ui_task_runner_installed() {
  return GlobalWindowlessTaskRunnerInstalled().load();
}

uint64_t HeadlessWindowlessRenderer::global_ui_tasks_posted() {
  return GlobalWindowlessTasksPosted().load();
}

uint64_t HeadlessWindowlessRenderer::global_ui_tasks_run() {
  return GlobalWindowlessTasksRun().load();
}

uint64_t HeadlessWindowlessRenderer::global_ui_tasks_failed() {
  return GlobalWindowlessTasksFailed().load();
}

}  // namespace lynxtron
