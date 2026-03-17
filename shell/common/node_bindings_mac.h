// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_NODE_BINDINGS_MAC_H_
#define LYNXTRON_SHELL_COMMON_NODE_BINDINGS_MAC_H_

#include "shell/common/node_bindings.h"

namespace lynxtron {

class NodeBindingsMac : public NodeBindings {
 public:
  NodeBindingsMac();

 private:
  // NodeBindings
  void PollEvents() override;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_NODE_BINDINGS_MAC_H_
