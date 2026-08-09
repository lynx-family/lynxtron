// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/testbench_record_fetcher.h"

#import <Foundation/Foundation.h>

namespace lynxtron {

void FetchTestbenchRecord(const std::string& url,
                          TestbenchRecordCallback callback) {
  if (!callback) {
    return;
  }
  NSString* url_string = [NSString stringWithUTF8String:url.c_str()];
  NSURL* ns_url = [NSURL URLWithString:url_string];
  if (!ns_url) {
    NSLog(@"Invalid testbench record url: %@", url_string);
    callback("");
    return;
  }

  if (ns_url.isFileURL) {
    NSError* error = nil;
    NSData* data = [NSData dataWithContentsOfURL:ns_url options:0 error:&error];
    if (!data || error) {
      NSLog(@"Failed to fetch testbench record: %@, error: %@", url_string,
            error);
      callback("");
      return;
    }
    callback(std::string(static_cast<const char*>(data.bytes), data.length));
    return;
  }

  NSURLSessionDataTask* task = [[NSURLSession sharedSession]
        dataTaskWithURL:ns_url
      completionHandler:^(NSData* data, NSURLResponse* response,
                          NSError* error) {
        if (error || data.length == 0) {
          NSLog(@"Failed to fetch testbench record: %@, error: %@", url_string,
                error);
          callback("");
          return;
        }
        NSLog(@"Fetched testbench record: %@, bytes: %lu", url_string,
              static_cast<unsigned long>(data.length));
        callback(
            std::string(static_cast<const char*>(data.bytes), data.length));
      }];
  [task resume];
}

}  // namespace lynxtron
