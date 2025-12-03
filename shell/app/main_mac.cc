// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "base/allocator/early_zone_registration_apple.h"
#include "shell/app/library_main.h"

int main(int argc, char* argv[]) {
  partition_alloc::EarlyMallocZoneRegistration();
  return LynxtronMain(argc, argv);

  // TODO(Guo Xi): relauncher
  // base::CommandLine::Init(argc, argv);
  // lynxtron::LynxtronCommandLine::Init(argc, argv);
  // constexpr char kRelauncherProcess[] = "relauncher";
  // base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // const std::string process_type =
  //     command_line->GetSwitchValueASCII(kProcessType);
  // if (process_type == kRelauncherProcess) {
  //   // TODO(Guo Xi): relauncher
  //   // relauncher::RelauncherMain();
  //   return 0;
  // } else {
  // }
}
