// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/testbench_record_fetcher.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "shell/common/thread_restrictions.h"

namespace lynxtron {

std::optional<std::string> FetchTestbenchRecord(const std::string& url) {
  NSString* url_string = [NSString stringWithUTF8String:url.c_str()];
  NSURL* ns_url = [NSURL URLWithString:url_string];
  if (!ns_url) {
    LOG(ERROR) << "Invalid testbench record url: " << url;
    return std::nullopt;
  }

  NSError* error = nil;
  NSData* data = nil;
  {
    ScopedAllowBlockingForLynxtron allow_blocking;
    data = [NSData dataWithContentsOfURL:ns_url options:0 error:&error];
  }
  if (!data || error) {
    LOG(ERROR) << "Failed to fetch testbench record: " << url;
    return std::nullopt;
  }

  const char* bytes = static_cast<const char*>([data bytes]);
  return std::string(bytes, bytes + [data length]);
}

}  // namespace lynxtron
