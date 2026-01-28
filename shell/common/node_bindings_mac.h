// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

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
