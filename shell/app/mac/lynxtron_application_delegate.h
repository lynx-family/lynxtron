// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_APP_MAC_LYNXTRON_APPLICATION_DELEGATE_H_
#define LYNXTRON_SHELL_APP_MAC_LYNXTRON_APPLICATION_DELEGATE_H_

#import <Cocoa/Cocoa.h>

#ifdef __cplusplus
namespace lynxtron {
class LynxtronMenuModel;
}
#endif

@interface LynxtronApplicationDelegate : NSObject <NSApplicationDelegate>

- (void)setApplicationDockMenu:(lynxtron::LynxtronMenuModel*)model;

@end

#endif  // LYNXTRON_SHELL_APP_MAC_LYNXTRON_APPLICATION_DELEGATE_H_
