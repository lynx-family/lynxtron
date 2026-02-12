// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "base/allocator/early_zone_registration_apple.h"
#include "shell/app/library_main.h"

int main(int argc, char* argv[]) {
  // TODO(Guo Xi): review EarlyMallocZoneRegistration
  // partition_alloc::EarlyMallocZoneRegistration();
  return LynxtronMain(argc, argv);
}
