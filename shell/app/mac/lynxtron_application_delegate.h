// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

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
