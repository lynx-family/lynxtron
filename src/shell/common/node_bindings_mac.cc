// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove FD_ZERO and convert code to safer
// constructs.
#pragma allow_unsafe_buffers
#endif

#include "shell/common/node_bindings_mac.h"

#include <errno.h>
#include <sys/select.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <memory>

namespace lynxtron {

NodeBindingsMac::NodeBindingsMac() = default;

void NodeBindingsMac::PollEvents() {
  auto* const event_loop = uv_loop();

  struct timeval tv;
  int timeout = uv_backend_timeout(event_loop);
  if (timeout != -1) {
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
  }

  fd_set readset;
  int fd = uv_backend_fd(event_loop);
  FD_ZERO(&readset);
  FD_SET(fd, &readset);

  // Wait for new libuv events.
  int r;
  do {
    r = select(fd + 1, &readset, nullptr, nullptr,
               timeout == -1 ? nullptr : &tv);
  } while (r == -1 && errno == EINTR);
}

// static
std::unique_ptr<NodeBindings> NodeBindings::Create() {
  return std::make_unique<NodeBindingsMac>();
}

}  // namespace lynxtron
