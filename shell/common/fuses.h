// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_FUSES_H_
#define LYNXTRON_SHELL_COMMON_FUSES_H_

#include <cstddef>
#include <cstdint>

namespace lynxtron::fuses {

enum class FuseV1Option : std::size_t {
  kRunAsNode = 0,
  kEnableNodeOptionsEnvironmentVariable,
  kEnableNodeCliInspectArguments,
  kEnableEmbeddedAsarIntegrityValidation,
  kOnlyLoadAppFromAsar,
};

inline constexpr uint8_t kFuseVersion = 1;
inline constexpr std::size_t kFuseCount = 5;

bool IsRunAsNodeEnabled();
bool IsNodeOptionsEnabled();
bool IsNodeCliInspectEnabled();
bool IsEmbeddedAsarIntegrityValidationEnabled();
bool IsOnlyLoadAppFromAsarEnabled();

}  // namespace lynxtron::fuses

#endif  // LYNXTRON_SHELL_COMMON_FUSES_H_
