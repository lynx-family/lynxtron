// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/windowless_renderer.h"

#include <chrono>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "lynx/platform/embedder/public/lynx_windowless_renderer.h"
#if BUILDFLAG(IS_LINUX)
#include "lynx/platform/embedder/public/capi/lynx_windowless_renderer_capi.h"
#endif
#include "shell/common/global_thread.h"

namespace lynxtron {
namespace {

#if BUILDFLAG(IS_LINUX)
bool RunsOnLynxtronUIThread(void* user_data) {
  return GlobalThread::CurrentlyOn(GlobalThread::UI);
}

void PostLynxWindowlessUITask(lynx_task_t task,
                              uint64_t target_time_nanoseconds,
                              void* user_data) {
  uint64_t now_nanoseconds = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
  base::TimeDelta delay =
      target_time_nanoseconds > now_nanoseconds
          ? base::Nanoseconds(target_time_nanoseconds - now_nanoseconds)
          : base::TimeDelta();

  GlobalThread::GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](lynx_task_t task) {
            static_cast<void>(lynx_windowless_run_ui_task(task));
          },
          task),
      delay);
}
#endif

class NoopWindowlessRenderer : public lynx::pub::LynxWindowlessRenderer {
 public:
  NoopWindowlessRenderer()
      : lynx::pub::LynxWindowlessRenderer(kRendererTypeSoftware) {}

  bool OnSoftwarePresent(const void* allocation,
                         size_t row_bytes,
                         size_t height) override {
    return true;
  }

  void OnPostTask(lynx_task_t task, uint64_t interval_nanoseconds) override {
    std::weak_ptr<lynx::pub::LynxWindowlessRenderer> renderer =
        weak_from_this();
    GlobalThread::GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](std::weak_ptr<lynx::pub::LynxWindowlessRenderer> renderer,
               lynx_task_t task) {
              if (auto locked_renderer = renderer.lock()) {
                locked_renderer->RunTask(task);
              }
            },
            std::move(renderer), task),
        base::Nanoseconds(interval_nanoseconds));
  }
};

}  // namespace

#if BUILDFLAG(IS_LINUX)
void InitializeWindowlessGlobalUITaskRunner() {
  lynx_windowless_ui_task_runner_config_t config = {};
  config.struct_size = sizeof(config);
  config.runs_on_current_thread_callback = &RunsOnLynxtronUIThread;
  config.post_task_callback = &PostLynxWindowlessUITask;
  lynx_windowless_set_global_ui_task_runner(&config);
}
#endif

std::shared_ptr<lynx::pub::LynxWindowlessRenderer> CreateWindowlessRenderer() {
  return std::make_shared<NoopWindowlessRenderer>();
}

}  // namespace lynxtron
