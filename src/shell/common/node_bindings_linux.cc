// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/node_bindings.h"

#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#include <memory>

namespace lynxtron {

class NodeBindingsLinux : public NodeBindings {
 public:
  NodeBindingsLinux() = default;

 private:
  void PollEvents() override {
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

    int r;
    do {
      r = select(fd + 1, &readset, nullptr, nullptr,
                 timeout == -1 ? nullptr : &tv);
    } while (r == -1 && errno == EINTR);
  }
};

std::unique_ptr<NodeBindings> NodeBindings::Create() {
  return std::make_unique<NodeBindingsLinux>();
}

}  // namespace lynxtron
