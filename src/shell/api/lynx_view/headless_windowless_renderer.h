// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_HEADLESS_WINDOWLESS_RENDERER_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_HEADLESS_WINDOWLESS_RENDERER_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "lynx/platform/embedder/public/lynx_windowless_renderer.h"

namespace lynxtron {

class HeadlessWindowlessRenderer final
    : public lynx::pub::LynxWindowlessRenderer {
 public:
  HeadlessWindowlessRenderer();
  ~HeadlessWindowlessRenderer() override;

  bool OnSoftwarePresent(const void* allocation,
                         size_t row_bytes,
                         size_t height) override;
  void OnPostTask(lynx_task_t task, uint64_t interval_nanoseconds) override;

  bool CopyLastFrameToPng(std::vector<uint8_t>* output) const;
  bool DispatchTextInput(const std::string& text);
  bool DispatchKeyEvent(const std::string& type,
                        uint64_t logical,
                        const std::string& character,
                        bool synthesized);
  uint64_t frames_presented() const;
  uint64_t tasks_posted() const;
  uint64_t tasks_run() const;

  static bool global_ui_task_runner_install_attempted();
  static bool global_ui_task_runner_installed();
  static uint64_t global_ui_tasks_posted();
  static uint64_t global_ui_tasks_run();
  static uint64_t global_ui_tasks_failed();

 private:
  mutable std::mutex frame_mutex_;
  std::vector<uint8_t> latest_frame_;
  size_t latest_row_bytes_ = 0;
  size_t latest_height_ = 0;
  uint64_t frames_presented_ = 0;
  std::atomic<uint64_t> tasks_posted_{0};
  std::atomic<uint64_t> tasks_run_{0};
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_HEADLESS_WINDOWLESS_RENDERER_H_
