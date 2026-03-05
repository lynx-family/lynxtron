// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_GENERIC_RESOURCE_FETCHER_FACTORY_H_
#define LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_GENERIC_RESOURCE_FETCHER_FACTORY_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "platform/embedder/public/lynx_generic_resource_fetcher.h"

namespace lynxtron {
namespace api {
class LynxWindow;
}  // namespace api

class LynxGenericResourceFetcherFactory {
 public:
  static std::shared_ptr<lynx::pub::LynxGenericResourceFetcher> Create(
      base::WeakPtr<api::LynxWindow> lynx_window);
};
}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_LYNX_RESOURCE_FETCHER_LYNX_GENERIC_RESOURCE_FETCHER_FACTORY_H_
