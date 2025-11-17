// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Copyright (c) 2013 Adam Roben <adam@roben.org>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE-CHROMIUM file.

#import "shell/common/mac/main_application_bundle.h"

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"

namespace lynxtron {
base::FilePath MainApplicationBundlePath() {
  // Start out with the path to the running executable.
  base::FilePath path;
  base::PathService::Get(base::FILE_EXE, &path);

  // Up to Contents.
  path = path.DirName().DirName();
  DCHECK_EQ(path.BaseName().value(), "Contents");

  // Up one more level to the .app.
  path = path.DirName();
  DCHECK_EQ(path.BaseName().Extension(), ".app");

  return path;
}

NSBundle* MainApplicationBundle() {
  return [NSBundle bundleWithPath:base::apple::FilePathToNSString(
                                      MainApplicationBundlePath())];
}

}  // namespace lynxtron
