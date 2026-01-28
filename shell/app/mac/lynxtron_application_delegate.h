// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_APP_MAC_LYNXTRON_APPLICATION_DELEGATE_H_
#define LYNXTRON_SHELL_APP_MAC_LYNXTRON_APPLICATION_DELEGATE_H_

#import <Cocoa/Cocoa.h>

// #import "shell/browser/ui/cocoa/electron_menu_controller.h"

@interface LynxtronApplicationDelegate : NSObject <NSApplicationDelegate>

// Sets the menu that will be returned in "applicationDockMenu:".
// - (void)setApplicationDockMenu:(electron::ElectronMenuModel*)model;

@end

#endif  // LYNXTRON_SHELL_APP_MAC_LYNXTRON_APPLICATION_DELEGATE_H_
