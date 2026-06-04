// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/main_parts.h"

#include "base/apple/bundle_locations.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "shell/api/lynx_view/lynx_view.h"
#include "shell/app/mac/lynxtron_application.h"
#include "shell/app/mac/lynxtron_application_delegate.h"

namespace lynxtron {

static LynxtronApplicationDelegate* __strong delegate_;

void MainParts::InitializeMacMainMessageLoop() {
  // Set our own application delegate.
  delegate_ = [[LynxtronApplicationDelegate alloc] init];
  [NSApp setDelegate:delegate_];

  InitializeMainNib();
  RegisterURLHandler();

  // Prevent Cocoa from turning command-line arguments into
  // |-application:openFiles:|, since we already handle them directly.
  [[NSUserDefaults standardUserDefaults]
      setObject:@"NO"
         forKey:@"NSTreatUnknownArgumentsAsOpen"];
}

void MainParts::FreeAppDelegate() {
  delegate_ = nil;
  [NSApp setDelegate:nil];
}

void MainParts::RegisterURLHandler() {
  [[LynxtronApplication sharedApplication] registerURLHandler];
}

// Replicates NSApplicationMain, but doesn't start a run loop.
void MainParts::InitializeMainNib() {
  auto infoDictionary = base::apple::OuterBundle().infoDictionary;

  auto principalClass =
      NSClassFromString([infoDictionary objectForKey:@"NSPrincipalClass"]);
  if (!principalClass) {
    principalClass = [LynxtronApplication class];
  }
  auto application = [principalClass sharedApplication];

  NSString* mainNibName = [infoDictionary objectForKey:@"NSMainNibFile"];

  NSNib* mainNib;

  @try {
    mainNib = [[NSNib alloc] initWithNibNamed:mainNibName
                                       bundle:base::apple::FrameworkBundle()];
    // Handle failure of initWithNibNamed on SMB shares
    // TODO(codebytere): Remove when
    // https://bugs.chromium.org/p/chromium/issues/detail?id=932935 is fixed
  } @catch (NSException* exception) {
    NSString* nibPath =
        [NSString stringWithFormat:@"Resources/%@.nib", mainNibName];
    nibPath = [base::apple::FrameworkBundle().bundlePath
        stringByAppendingPathComponent:nibPath];

    NSData* data = [NSData dataWithContentsOfFile:nibPath];
    mainNib = [[NSNib alloc] initWithNibData:data
                                      bundle:base::apple::FrameworkBundle()];
  }

  [mainNib instantiateWithOwner:application topLevelObjects:nil];
}

void MainParts::RegisterAtomCrApp() {
  // Force the NSApplication subclass to be used.
  [LynxtronApplication sharedApplication];
}

}  // namespace lynxtron
