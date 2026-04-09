// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_FILE_URL_UTIL_H_
#define LYNXTRON_SHELL_COMMON_FILE_URL_UTIL_H_

#include "base/files/file_path.h"
#include "url/gurl.h"

namespace lynxtron {

// Converts a file:// URL into a local file path.
//
// This is adapted from Chromium's file URL handling so callers do not need to
// treat file URLs as raw strings. The conversion performs percent-decoding and
// preserves platform-specific path semantics such as Windows drive letters.
//
// Reference:
// https://source.chromium.org/chromium/chromium/src/+/main:net/base/filename_util.cc
bool FileURLToFilePath(const GURL& url, base::FilePath* file_path);

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_FILE_URL_UTIL_H_
