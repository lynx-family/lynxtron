// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/windowless_renderer.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "lynx/platform/embedder/public/lynx_windowless_renderer.h"
#include "shell/common/global_thread.h"

namespace lynxtron {
namespace {

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

std::shared_ptr<lynx::pub::LynxWindowlessRenderer> CreateWindowlessRenderer() {
  return std::make_shared<NoopWindowlessRenderer>();
}

}  // namespace lynxtron
