// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_PROCESS_SINGLETON_PROCESS_SINGLETON_INTERNAL_H_
#define LYNXTRON_SHELL_COMMON_PROCESS_SINGLETON_PROCESS_SINGLETON_INTERNAL_H_

#include "shell/common/process_singleton/process_singleton.h"

namespace internal {

void SendRemoteProcessInteractionResultHistogram(
    ProcessSingleton::RemoteProcessInteractionResult result);

void SendRemoteHungProcessTerminateReasonHistogram(
    ProcessSingleton::RemoteHungProcessTerminateReason reason);

}  // namespace internal

#endif  // LYNXTRON_SHELL_COMMON_PROCESS_SINGLETON_PROCESS_SINGLETON_INTERNAL_H_
