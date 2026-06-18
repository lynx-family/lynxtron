// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/functional/callback.h"
#include "base/strings/sys_string_conversions.h"
#include "shell/api/api_app.h"

namespace lynxtron::api {

base::RepeatingCallback<void()> App::StartAccessingSecurityScopedResource(
    gin::Arguments* args) {
  std::string data;
  if (!args->GetNext(&data)) {
    args->ThrowTypeError("bookmarkData must be a string");
    return {};
  }

  NSString* base64_string = base::SysUTF8ToNSString(data);
  NSData* bookmark_data =
      [[NSData alloc] initWithBase64EncodedString:base64_string options:0];
  if (bookmark_data == nil) {
    gin_helper::ErrorThrower(args->isolate())
        .ThrowError("Failed to decode bookmark data");
    return {};
  }

  BOOL is_stale = false;
  NSError* error = nil;
  NSURL* bookmark_url = [NSURL
      URLByResolvingBookmarkData:bookmark_data
                         options:NSURLBookmarkResolutionWithSecurityScope |
                                 NSURLBookmarkResolutionWithoutMounting
                   relativeToURL:nil
             bookmarkDataIsStale:&is_stale
                           error:&error];

  if (error != nil) {
    NSString* err =
        [NSString stringWithFormat:@"NSError: %@ %@", error, [error userInfo]];
    gin_helper::ErrorThrower(args->isolate())
        .ThrowError(base::SysNSStringToUTF8(err));
    return {};
  }

  if (is_stale) {
    gin_helper::ErrorThrower(args->isolate())
        .ThrowError("bookmarkDataIsStale - try recreating the bookmark");
    return {};
  }

  if (bookmark_url == nil ||
      ![bookmark_url startAccessingSecurityScopedResource]) {
    gin_helper::ErrorThrower(args->isolate())
        .ThrowError("Failed to access security scoped resource");
    return {};
  }

  NSURL* __strong scoped_bookmark_url = bookmark_url;
  return base::BindRepeating(^{
    [scoped_bookmark_url stopAccessingSecurityScopedResource];
  });
}

}  // namespace lynxtron::api
