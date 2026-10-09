// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Copyright (c) 2013 Adam Roben <adam@roben.org>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE-CHROMIUM file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import "shell/common/mac/main_application_bundle.h"

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"

namespace lynxtron {
base::FilePath MainApplicationBundlePath() {
  // Start out with the path to the running executable.
  base::FilePath exe_path;
  base::PathService::Get(base::FILE_EXE, &exe_path);

  // Up to MacOS.
  base::FilePath macos_dir = exe_path.DirName();
  DCHECK_EQ(macos_dir.BaseName().value(), "MacOS");

  // Up to Contents.
  base::FilePath contents_dir = macos_dir.DirName();
  DCHECK_EQ(contents_dir.BaseName().value(), "Contents");

  // Up one more level to the current .app.
  base::FilePath current_app = contents_dir.DirName();
  DCHECK_EQ(current_app.BaseName().Extension(), ".app");

  // If the current app is nested under:
  // Outer.app/Contents/Frameworks/Helper.app
  // return the outer app instead of the helper app.
  base::FilePath maybe_frameworks_dir = current_app.DirName();
  if (maybe_frameworks_dir.BaseName().value() == "Frameworks") {
    base::FilePath maybe_outer_contents = maybe_frameworks_dir.DirName();
    if (maybe_outer_contents.BaseName().value() == "Contents") {
      base::FilePath outer_app = maybe_outer_contents.DirName();
      if (outer_app.BaseName().Extension() == ".app") {
        return outer_app;
      }
    }
  }

  return current_app;
}

NSBundle* MainApplicationBundle() {
  return [NSBundle bundleWithPath:base::apple::FilePathToNSString(
                                      MainApplicationBundlePath())];
}

}  // namespace lynxtron
