// Copyright (c) 2024 Lynxtron Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shell/app/main_runner.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "shell/app/main_parts.h"
#include "shell/app/uv_stdio_fix.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "shell/common/mac/main_application_bundle.h"
#endif

namespace lynxtron {

namespace {

void InitializePlatform() {
#if BUILDFLAG(IS_MAC)
  FixStdioStreams();
  base::apple::SetOverrideFrameworkBundlePath(
      lynxtron::MainApplicationBundlePath()
          .Append("Contents")
          .Append("Frameworks")
          .Append(LYNXTRON_PRODUCT_NAME " Framework.framework"));
#endif
}

}  // namespace

// static
std::unique_ptr<MainRunner> MainRunner::Create() {
  return base::WrapUnique(new MainRunner());
}

MainRunner::MainRunner() = default;

MainRunner::~MainRunner() = default;

int MainRunner::Initialize() {
  InitializePlatform();
  return 0;
}

int MainRunner::Run() {
  main_parts_ = std::make_unique<MainParts>();
  main_parts_->Initialize();

  auto run_loop =
      std::make_unique<base::RunLoop>(base::RunLoop::Type::kDefault);
  main_parts_->WillRunMainMessageLoop(run_loop);
  run_loop->Run();
  main_parts_->PostMainMessageLoopRun();
  return main_parts_->GetExitCode();
}

void MainRunner::Shutdown() {
  main_parts_->Shutdown();
}

}  // namespace lynxtron
