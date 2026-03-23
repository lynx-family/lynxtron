// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_COCOA_LYNXTRON_MENU_CONTROLLER_H_
#define LYNXTRON_SHELL_UI_COCOA_LYNXTRON_MENU_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

namespace lynxtron {
class LynxtronMenuModel;
}

@interface LynxtronMenuController
    : NSObject <NSMenuDelegate, NSSharingServiceDelegate> {
 @protected
  base::WeakPtr<lynxtron::LynxtronMenuModel> model_;
  NSMenu* __strong menu_;
  BOOL isMenuOpen_;
  NSInteger openMenuCount_;
  BOOL useDefaultAccelerator_;
  base::OnceClosure popupCloseCallback;
}

- (id)initWithModel:(lynxtron::LynxtronMenuModel*)model
    useDefaultAccelerator:(BOOL)use;

- (void)setPopupCloseCallback:(base::OnceClosure)callback;
- (void)populateWithModel:(lynxtron::LynxtronMenuModel*)model;
- (void)cancel;
- (void)populateMenu:(NSMenu*)menu
           withModel:(lynxtron::LynxtronMenuModel*)model;

- (lynxtron::LynxtronMenuModel*)model;
- (void)setModel:(lynxtron::LynxtronMenuModel*)model;
- (NSMenu*)menu;

- (NSMenuItem*)makeMenuItemForIndex:(NSInteger)index
                          fromModel:(lynxtron::LynxtronMenuModel*)model;

- (BOOL)isMenuOpen;
- (void)refreshMenuTree:(NSMenu*)menu;

- (void)menuWillOpen:(NSMenu*)menu;
- (void)menuDidClose:(NSMenu*)menu;

@end

#endif  // LYNXTRON_SHELL_UI_COCOA_LYNXTRON_MENU_CONTROLLER_H_
