// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_LYNX_HTTP_SERVICE_LYNX_HTTP_SERVICE_H_
#define LYNXTRON_SHELL_LYNX_HTTP_SERVICE_LYNX_HTTP_SERVICE_H_

namespace lynxtron {

// Registers the HTTP service that backs Lynx `fetch()` with the shared
// LynxServiceCenter. The concrete provider is selected at build time through
// the `lynxtron_http_service_deps` GN seam: the open-source build links a
// transport backed by the platform's native networking stack, while the
// closed-source build links its own native transport. Both expose this same
// entry point so the call site stays provider agnostic.
void RegisterLynxHttpService();

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_LYNX_HTTP_SERVICE_LYNX_HTTP_SERVICE_H_
