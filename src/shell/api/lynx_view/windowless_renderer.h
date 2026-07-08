// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_WINDOWLESS_RENDERER_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_WINDOWLESS_RENDERER_H_

#include <memory>

namespace lynx {
namespace pub {
class LynxWindowlessRenderer;
}  // namespace pub
}  // namespace lynx

namespace lynxtron {

std::shared_ptr<lynx::pub::LynxWindowlessRenderer> CreateWindowlessRenderer();

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_WINDOWLESS_RENDERER_H_
