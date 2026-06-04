// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/fuses.h"

#include "base/check.h"

namespace lynxtron::fuses {
namespace {

constexpr char kEnabledFuseValue = '1';
constexpr char kDisabledFuseValue = '0';
constexpr std::size_t kSentinelLength = 32;
constexpr std::size_t kVersionOffset = kSentinelLength;
constexpr std::size_t kLengthOffset = kVersionOffset + 1;
constexpr std::size_t kWireOffset = kLengthOffset + 1;

// Keep the sentinel + version + wire bytes contiguous so package-time tooling
// can locate and patch the fuse wire in a built binary.
alignas(1) char g_fuse_wire[] = {
    'd',
    'L',
    '7',
    'p',
    'K',
    'G',
    'd',
    'n',
    'N',
    'z',
    '7',
    '9',
    '6',
    'P',
    'b',
    'b',
    'j',
    'Q',
    'W',
    'N',
    'K',
    'm',
    'H',
    'X',
    'B',
    'Z',
    'a',
    'B',
    '9',
    't',
    's',
    'X',
    static_cast<char>(kFuseVersion),
    static_cast<char>(kFuseCount),
    kEnabledFuseValue,
    kEnabledFuseValue,
    kEnabledFuseValue,
    kDisabledFuseValue,
    kDisabledFuseValue,
};

static_assert(sizeof(g_fuse_wire) == kWireOffset + kFuseCount);

bool IsFuseEnabled(FuseV1Option option) {
  const std::size_t index = static_cast<std::size_t>(option);
  CHECK(index < kFuseCount);
  CHECK(static_cast<unsigned char>(g_fuse_wire[kVersionOffset]) ==
        kFuseVersion);
  CHECK(static_cast<unsigned char>(g_fuse_wire[kLengthOffset]) == kFuseCount);

  return g_fuse_wire[kWireOffset + index] == kEnabledFuseValue;
}

}  // namespace

bool IsRunAsNodeEnabled() {
  return IsFuseEnabled(FuseV1Option::kRunAsNode);
}

bool IsNodeOptionsEnabled() {
  return IsFuseEnabled(FuseV1Option::kEnableNodeOptionsEnvironmentVariable);
}

bool IsNodeCliInspectEnabled() {
  return IsFuseEnabled(FuseV1Option::kEnableNodeCliInspectArguments);
}

bool IsEmbeddedAsarIntegrityValidationEnabled() {
  return IsFuseEnabled(FuseV1Option::kEnableEmbeddedAsarIntegrityValidation);
}

bool IsOnlyLoadAppFromAsarEnabled() {
  return IsFuseEnabled(FuseV1Option::kOnlyLoadAppFromAsar);
}

}  // namespace lynxtron::fuses
