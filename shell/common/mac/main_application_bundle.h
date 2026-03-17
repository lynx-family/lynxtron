// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Copyright (c) 2013 Adam Roben <adam@roben.org>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE-CHROMIUM file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_MAC_MAIN_APPLICATION_BUNDLE_H_
#define LYNXTRON_SHELL_COMMON_MAC_MAIN_APPLICATION_BUNDLE_H_

#ifdef __OBJC__
@class NSBundle;
#else
struct NSBundle;
#endif

namespace base {
class FilePath;
}

namespace lynxtron {

// The "main" application bundle is the outermost bundle for this logical
// application. E.g., if you have MyApp.app and
// MyApp.app/Contents/Frameworks/MyApp Helper.app, the main application bundle
// is MyApp.app, no matter which executable is currently running.
NSBundle* MainApplicationBundle();
base::FilePath MainApplicationBundlePath();

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_MAC_MAIN_APPLICATION_BUNDLE_H_
