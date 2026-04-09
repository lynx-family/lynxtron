// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/file_url_util.h"

// Adapted from Chromium's file URL conversion logic:
// https://source.chromium.org/chromium/chromium/src/+/main:net/base/filename_util.cc

#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/strings/escape.h"
#include "build/build_config.h"

namespace lynxtron {
namespace {

std::string DecodeURLPath(std::string_view escaped_path) {
  return base::UnescapeURLComponent(
      escaped_path,
      base::UnescapeRule::NORMAL | base::UnescapeRule::SPACES |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
}

#if BUILDFLAG(IS_WIN)
bool IsWindowsDrivePath(std::string_view path) {
  return path.size() >= 3 && path[0] == '/' &&
         ((path[1] >= 'A' && path[1] <= 'Z') ||
          (path[1] >= 'a' && path[1] <= 'z')) &&
         path[2] == ':';
}
#endif

}  // namespace

bool FileURLToFilePath(const GURL& url, base::FilePath* file_path) {
  if (!file_path || !url.is_valid() || !url.SchemeIsFile()) {
    return false;
  }

  std::string decoded_path = DecodeURLPath(url.path_piece());

#if BUILDFLAG(IS_WIN)
  std::string path_string;
  if (url.host_piece().empty() || url.host_piece() == "localhost") {
    path_string = decoded_path;
    if (IsWindowsDrivePath(path_string)) {
      path_string.erase(0, 1);
    }
  } else {
    path_string = "\\\\";
    path_string.append(url.host_piece());
    path_string.append(decoded_path);
  }

  if (path_string.empty()) {
    return false;
  }

  *file_path = base::FilePath::FromUTF8Unsafe(path_string);
  return true;
#else
  if (!url.host_piece().empty() && url.host_piece() != "localhost") {
    return false;
  }
  if (decoded_path.empty()) {
    return false;
  }
  *file_path = base::FilePath::FromUTF8Unsafe(decoded_path);
  return true;
#endif
}

}  // namespace lynxtron
