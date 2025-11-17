// Copyright (c) 2024 Lynxtron Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_APP_MAIN_RUNNER_H_
#define LYNXTRON_SHELL_APP_MAIN_RUNNER_H_

#include <memory>

namespace lynxtron {

class MainParts;

class MainRunner {
 public:
  static std::unique_ptr<MainRunner> Create();
  ~MainRunner();

  // Non-copyable
  MainRunner(const MainRunner&) = delete;
  MainRunner& operator=(const MainRunner&) = delete;

  int Initialize();
  int Run();
  void Shutdown();

 private:
  MainRunner();

  std::unique_ptr<MainParts> main_parts_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_MAIN_RUNNER_H_
