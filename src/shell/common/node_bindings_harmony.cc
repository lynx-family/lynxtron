// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.
//
// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// HarmonyOS NodeBindings.
//
// Unlike the macOS implementation (which uses select()), HarmonyOS uses
// poll() here: OHOS select() can miss wakeups for the libuv backend fd (an
// epoll fd on Linux), stalling uv IO callbacks (TCP connects, readable data)
// for seconds; poll() reports this fd reliably (verified on device: network
// latency dropped from ~15-26s to <1s).

#include "shell/common/node_bindings_harmony.h"

#include <errno.h>
#include <poll.h>
#include <sys/types.h>

#include <memory>

namespace lynxtron {

NodeBindingsHarmony::NodeBindingsHarmony() = default;

void NodeBindingsHarmony::PollEvents() {
  auto* const event_loop = uv_loop();

  // On HarmonyOS select() can miss wakeups for the uv backend fd (an epoll
  // fd on Linux), starving uv IO callbacks (TCP connects, readable data) for
  // seconds; poll() reports this fd reliably, so use it here. A timeout of -1
  // (uv has no pending timers) is fine: poll blocks until an IO event arrives.
  const int fd = uv_backend_fd(event_loop);
  const int timeout = uv_backend_timeout(event_loop);  // ms; -1 = wait indefinitely

  struct pollfd pf;
  pf.fd = fd;
  pf.events = POLLIN;
  pf.revents = 0;

  int r;
  do {
    r = poll(&pf, 1, timeout);
  } while (r == -1 && errno == EINTR);
}

// static
std::unique_ptr<NodeBindings> NodeBindings::Create() {
  return std::make_unique<NodeBindingsHarmony>();
}

}  // namespace lynxtron
