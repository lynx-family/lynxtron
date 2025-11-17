// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_COMMON_NODE_BINDINGS_WIN_H_
#define ELECTRON_SHELL_COMMON_NODE_BINDINGS_WIN_H_

#include "shell/common/node_bindings.h"

namespace lynxtron {

class NodeBindingsWin : public NodeBindings {
 public:
  explicit NodeBindingsWin();

 private:
  // NodeBindings
  void PollEvents() override;
};

}  // namespace lynxtron

#endif  // ELECTRON_SHELL_COMMON_NODE_BINDINGS_WIN_H_
