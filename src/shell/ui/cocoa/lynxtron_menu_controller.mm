// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import "shell/ui/cocoa/lynxtron_menu_controller.h"

#include <string>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "shell/ui/lynxtron_menu_model.h"
#include "url/gurl.h"

using SharingItem = lynxtron::LynxtronMenuModel::SharingItem;

namespace {

struct Role {
  SEL selector;
  const char* role;
};

Role kRolesMap[] = {
    {@selector(orderFrontStandardAboutPanel:), "about"},
    {@selector(hide:), "hide"},
    {@selector(hideOtherApplications:), "hideothers"},
    {@selector(unhideAllApplications:), "unhide"},
    {@selector(arrangeInFront:), "front"},
    {@selector(undo:), "undo"},
    {@selector(redo:), "redo"},
    {@selector(cut:), "cut"},
    {@selector(copy:), "copy"},
    {@selector(paste:), "paste"},
    {@selector(delete:), "delete"},
    {@selector(pasteAndMatchStyle:), "pasteandmatchstyle"},
    {@selector(selectAll:), "selectall"},
    {@selector(orderFrontSubstitutionsPanel:), "showsubstitutions"},
    {@selector(toggleAutomaticQuoteSubstitution:), "togglesmartquotes"},
    {@selector(toggleAutomaticDashSubstitution:), "togglesmartdashes"},
    {@selector(toggleAutomaticTextReplacement:), "toggletextreplacement"},
    {@selector(startSpeaking:), "startspeaking"},
    {@selector(stopSpeaking:), "stopspeaking"},
    {@selector(performMiniaturize:), "minimize"},
    {@selector(performClose:), "close"},
    {@selector(performZoom:), "zoom"},
    {@selector(terminate:), "quit"},
    {@selector(toggleFullScreenMode:), "togglefullscreen"},
    {@selector(toggleTabBar:), "toggletabbar"},
    {@selector(selectNextTab:), "selectnexttab"},
    {@selector(toggleTabOverview:), "showalltabs"},
    {@selector(selectPreviousTab:), "selectprevioustab"},
    {@selector(mergeAllWindows:), "mergeallwindows"},
    {@selector(moveTabToNewWindow:), "movetabtonewwindow"},
    {@selector(clearRecentDocuments:), "clearrecentdocuments"},
};

bool MenuHasVisibleItems(const lynxtron::LynxtronMenuModel* model) {
  size_t count = model->GetItemCount();
  for (size_t index = 0; index < count; index++) {
    if (model->IsVisibleAt(index)) {
      return true;
    }
  }
  return false;
}

NSMenu* MakeEmptySubmenu() {
  NSMenu* submenu = [[NSMenu alloc] initWithTitle:@""];
  submenu.autoenablesItems = NO;
  [submenu addItemWithTitle:@"(empty)" action:nullptr keyEquivalent:@""];
  [[submenu itemAtIndex:0] setEnabled:NO];
  return submenu;
}

NSArray* ConvertSharingItemToNS(const SharingItem& item) {
  NSMutableArray* result = [NSMutableArray array];
  if (item.texts) {
    for (const std::string& str : *item.texts) {
      [result addObject:base::SysUTF8ToNSString(str)];
    }
  }
  if (item.file_paths) {
    for (const base::FilePath& path : *item.file_paths) {
      [result addObject:base::apple::FilePathToNSURL(path)];
    }
  }
  if (item.urls) {
    for (const GURL& url : *item.urls) {
      [result
          addObject:[NSURL URLWithString:base::SysUTF8ToNSString(url.spec())]];
    }
  }
  return result;
}

NSString* KeyEquivalentFromAccelerator(const ui::Accelerator& accelerator) {
  if (accelerator.IsEmpty()) {
    return @"";
  }
  std::string key = accelerator.key();
  if (key.size() == 1) {
    // NSMenuItem keyEquivalent must be lowercase; uppercase implies Shift
    // modifier.
    std::string lower = base::ToLowerASCII(key);
    return [NSString stringWithUTF8String:lower.c_str()];
  }
  std::string lower = base::ToLowerASCII(key);
  if (lower == "tab") {
    return @"\t";
  }
  if (lower == "space") {
    return @" ";
  }
  if (lower == "enter" || lower == "return") {
    return @"\r";
  }
  if (lower == "escape" || lower == "esc") {
    return @"\e";
  }
  return @"";
}

NSUInteger ModifierMaskFromAccelerator(const ui::Accelerator& accelerator) {
  NSUInteger mask = 0;
  if (accelerator.IsCtrlDown()) {
    mask |= NSEventModifierFlagControl;
  }
  if (accelerator.IsAltDown()) {
    mask |= NSEventModifierFlagOption;
  }
  if (accelerator.IsCmdDown()) {
    mask |= NSEventModifierFlagCommand;
  }
  if (accelerator.IsShiftDown()) {
    mask |= NSEventModifierFlagShift;
  }
  return mask;
}

int EventFlagsFromCurrentEvent() {
  NSEvent* event = NSApp.currentEvent;
  if (!event) {
    return 0;
  }

  int flags = 0;
  NSEventModifierFlags modifiers = event.modifierFlags;
  if (modifiers & NSEventModifierFlagShift) {
    flags |= ui::Accelerator::kShift;
  }
  if (modifiers & NSEventModifierFlagControl) {
    flags |= ui::Accelerator::kCtrl;
  }
  if (modifiers & NSEventModifierFlagOption) {
    flags |= ui::Accelerator::kAlt;
  }
  if (modifiers & NSEventModifierFlagCommand) {
    flags |= ui::Accelerator::kCmd;
  }
  return flags;
}

}  // namespace

@interface WeakPtrToLynxtronMenuModelAsNSObject : NSObject
+ (instancetype)weakPtrForModel:(lynxtron::LynxtronMenuModel*)model;
+ (lynxtron::LynxtronMenuModel*)getFrom:(id)instance;
- (instancetype)initWithModel:(lynxtron::LynxtronMenuModel*)model;
- (lynxtron::LynxtronMenuModel*)menuModel;
@end

@implementation WeakPtrToLynxtronMenuModelAsNSObject {
  base::WeakPtr<lynxtron::LynxtronMenuModel> _model;
}

+ (instancetype)weakPtrForModel:(lynxtron::LynxtronMenuModel*)model {
  return [[WeakPtrToLynxtronMenuModelAsNSObject alloc] initWithModel:model];
}

+ (lynxtron::LynxtronMenuModel*)getFrom:(id)instance {
  return [base::apple::ObjCCastStrict<WeakPtrToLynxtronMenuModelAsNSObject>(
      instance) menuModel];
}

- (instancetype)initWithModel:(lynxtron::LynxtronMenuModel*)model {
  if ((self = [super init])) {
    _model = model->GetWeakPtr();
  }
  return self;
}

- (lynxtron::LynxtronMenuModel*)menuModel {
  return _model.get();
}

@end

@interface LynxtronMenuController ()
- (void)performShare:(id)sender;
- (lynxtron::LynxtronMenuModel*)modelForMenu:(NSMenu*)menu;
- (void)finalizeTrackingIfNeeded;
- (void)applyDisplayAttributesToMenuItem:(NSMenuItem*)item;
@end

@implementation LynxtronMenuController

- (lynxtron::LynxtronMenuModel*)model {
  return model_.get();
}

- (void)setModel:(lynxtron::LynxtronMenuModel*)model {
  model_ = model->GetWeakPtr();
}

- (instancetype)initWithModel:(lynxtron::LynxtronMenuModel*)model
        useDefaultAccelerator:(BOOL)use {
  if ((self = [super init])) {
    model_ = model->GetWeakPtr();
    isMenuOpen_ = NO;
    isClosing_ = NO;
    pendingClose_ = NO;
    openMenuCount_ = 0;
    useDefaultAccelerator_ = use;
    [self menu];
  }
  return self;
}

- (void)dealloc {
  [menu_ setDelegate:nil];
  [self cancel];
  model_ = nullptr;
}

- (void)setPopupCloseCallback:(base::OnceClosure)callback {
  popupCloseCallback = std::move(callback);
}

- (void)populateWithModel:(lynxtron::LynxtronMenuModel*)model {
  if (!menu_) {
    return;
  }
  model_ = model->GetWeakPtr();
  [self populateMenu:menu_ withModel:model];
}

- (void)populateMenu:(NSMenu*)menu
           withModel:(lynxtron::LynxtronMenuModel*)model {
  [menu removeAllItems];

  const size_t count = model->GetItemCount();
  for (size_t index = 0; index < count; index++) {
    if (model->GetTypeAt(index) ==
        lynxtron::LynxtronMenuModel::TYPE_SEPARATOR) {
      [self addSeparatorToMenu:menu atIndex:index];
    } else {
      [self addItemToMenu:menu atIndex:index fromModel:model];
    }
  }
}

- (void)cancel {
  if (isMenuOpen_) {
    if (isClosing_) {
      return;
    }

    isClosing_ = YES;
    pendingClose_ = NO;
    [menu_ cancelTracking];
    isMenuOpen_ = NO;

    if (model_) {
      model_->MenuWillClose();
    }

    if (!popupCloseCallback.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(popupCloseCallback));
    }
  } else if (!popupCloseCallback.is_null()) {
    pendingClose_ = YES;
    [menu_ cancelTracking];
  }
}

- (NSMenu*)menuFromModel:(lynxtron::LynxtronMenuModel*)model {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@""];
  menu.autoenablesItems = NO;
  [self populateMenu:menu withModel:model];
  menu.delegate = self;
  return menu;
}

- (void)addSeparatorToMenu:(NSMenu*)menu atIndex:(NSInteger)index {
  NSMenuItem* separator = [NSMenuItem separatorItem];
  [menu insertItem:separator atIndex:index];
}

- (NSMenu*)createShareMenuForItem:(const SharingItem&)item {
  NSArray* items = ConvertSharingItemToNS(item);
  if ([items count] == 0) {
    return MakeEmptySubmenu();
  }
  NSMenu* menu = [[NSMenu alloc] init];
  menu.autoenablesItems = NO;
  NSArray* services = [NSSharingService sharingServicesForItems:items];
  for (NSSharingService* service in services) {
    [menu addItem:[self menuItemForService:service withItems:items]];
  }
  [menu setDelegate:self];
  return menu;
}

- (NSMenuItem*)menuItemForService:(NSSharingService*)service
                        withItems:(NSArray*)items {
  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:service.menuItemTitle
                                                action:@selector(performShare:)
                                         keyEquivalent:@""];
  [item setTarget:self];
  [item setImage:service.image];
  [item setRepresentedObject:@{@"service" : service, @"items" : items}];
  return item;
}

- (void)performShare:(id)sender {
  NSMenuItem* menuItem = base::apple::ObjCCastStrict<NSMenuItem>(sender);
  id represented = menuItem.representedObject;
  if (![represented isKindOfClass:[NSDictionary class]]) {
    return;
  }
  NSDictionary* payload =
      base::apple::ObjCCastStrict<NSDictionary>(represented);
  NSSharingService* service = payload[@"service"];
  NSArray* items = payload[@"items"];
  if (![service isKindOfClass:[NSSharingService class]] ||
      ![items isKindOfClass:[NSArray class]] || [items count] == 0) {
    return;
  }
  service.delegate = self;
  [service performWithItems:items];
}

- (NSMenuItem*)makeMenuItemForIndex:(NSInteger)index
                          fromModel:(lynxtron::LynxtronMenuModel*)model {
  std::u16string label16 = model->GetLabelAt(index);
  std::u16string rawSecondaryLabel = model->GetSecondaryLabelAt(index);
  NSString* label = base::SysUTF16ToNSString(label16);

  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:label
                                                action:@selector(itemSelected:)
                                         keyEquivalent:@""];

  if (!rawSecondaryLabel.empty()) {
    if (@available(macOS 14.4, *)) {
      NSString* secondaryLabel = base::SysUTF16ToNSString(rawSecondaryLabel);
      if ([item respondsToSelector:@selector(setSubtitle:)]) {
        [item setValue:secondaryLabel forKey:@"subtitle"];
      }
    }
  }

  std::u16string role = model->GetRoleAt(index);
  std::u16string customType = model->GetCustomTypeAt(index);
  lynxtron::LynxtronMenuModel::ItemType type = model->GetTypeAt(index);

  if (@available(macOS 14, *)) {
    if (customType == u"header") {
      item = [NSMenuItem sectionHeaderWithTitle:label];
    }
  }

  gfx::Image icon = model->GetIconAt(index);
  if (!icon.IsEmpty()) {
    [item setImage:icon.ToNSImage()];
  }

  std::u16string toolTip = model->GetToolTipAt(index);
  [item setToolTip:base::SysUTF16ToNSString(toolTip)];

  if (role == u"services") {
    item.target = nil;
    item.action = nil;
    NSMenu* submenu =
        [[NSMenu alloc] initWithTitle:base::SysUTF16ToNSString(label16)];
    item.submenu = submenu;
    [NSApp setServicesMenu:submenu];
  } else if (role == u"sharemenu") {
    SharingItem sharing_item;
    bool has_sharing_item = model->GetSharingItemAt(index, &sharing_item);
    item.target = nil;
    item.action = nil;
    [item setSubmenu:has_sharing_item
                         ? [self createShareMenuForItem:sharing_item]
                         : MakeEmptySubmenu()];
  } else if (type == lynxtron::LynxtronMenuModel::TYPE_SUBMENU &&
             model->IsVisibleAt(index)) {
    item.target = nil;
    item.action = nil;
    lynxtron::LynxtronMenuModel* submenuModel = model->GetSubmenuModelAt(index);
    NSMenu* submenu = MenuHasVisibleItems(submenuModel)
                          ? [self menuFromModel:submenuModel]
                          : MakeEmptySubmenu();
    if (@available(macOS 14, *)) {
      if (customType == u"palette") {
        submenu.presentationStyle = NSMenuPresentationStylePalette;
      }
    }
    submenu.title = item.title;
    item.submenu = submenu;
    item.tag = index;
    item.representedObject =
        [WeakPtrToLynxtronMenuModelAsNSObject weakPtrForModel:model];
    submenu.delegate = self;

    if (role == u"window" || role == u"windowmenu") {
      [NSApp setWindowsMenu:submenu];
    } else if (role == u"help") {
      [NSApp setHelpMenu:submenu];
    }
  } else {
    item.tag = index;
    item.representedObject =
        [WeakPtrToLynxtronMenuModelAsNSObject weakPtrForModel:model];
    ui::Accelerator accelerator;
    if (model->GetAcceleratorAtWithParams(index, useDefaultAccelerator_,
                                          &accelerator)) {
      NSString* key = KeyEquivalentFromAccelerator(accelerator);
      if ([key length] != 0) {
        item.keyEquivalent = key;
        item.keyEquivalentModifierMask =
            ModifierMaskFromAccelerator(accelerator);
      }
    }

    if ([item
            respondsToSelector:@selector(setAllowsKeyEquivalentWhenHidden:)]) {
      [(id)item
          setAllowsKeyEquivalentWhenHidden:model->WorksWhenHiddenAt(index)];
    }

    item.target = self;
    if (!role.empty()) {
      for (const Role& pair : kRolesMap) {
        if (role == base::ASCIIToUTF16(pair.role)) {
          item.target = nil;
          item.action = pair.selector;
          break;
        }
      }
    }
  }

  return item;
}

- (void)applyStateToMenuItem:(NSMenuItem*)item {
  id represented = item.representedObject;
  if (!represented) {
    return;
  }

  if (![represented
          isKindOfClass:[WeakPtrToLynxtronMenuModelAsNSObject class]]) {
    return;
  }

  lynxtron::LynxtronMenuModel* model =
      [WeakPtrToLynxtronMenuModelAsNSObject getFrom:represented];
  if (!model) {
    return;
  }

  NSInteger index = item.tag;
  size_t count = model->GetItemCount();
  if (index < 0 || static_cast<size_t>(index) >= count) {
    return;
  }

  [self applyDisplayAttributesToMenuItem:item];
  item.enabled = model->IsEnabledAt(index);
  item.hidden = !model->IsVisibleAt(index);
  item.state = model->IsItemCheckedAt(index) ? NSControlStateValueOn
                                             : NSControlStateValueOff;
}

- (void)applyDisplayAttributesToMenuItem:(NSMenuItem*)item {
  lynxtron::LynxtronMenuModel* model =
      [WeakPtrToLynxtronMenuModelAsNSObject getFrom:item.representedObject];
  if (!model) {
    return;
  }

  NSInteger index = item.tag;
  size_t count = model->GetItemCount();
  if (index < 0 || static_cast<size_t>(index) >= count) {
    return;
  }

  std::u16string label16 = model->GetLabelAt(index);
  item.title = base::SysUTF16ToNSString(label16);

  std::u16string toolTip = model->GetToolTipAt(index);
  item.toolTip = base::SysUTF16ToNSString(toolTip);

  gfx::Image icon = model->GetIconAt(index);
  item.image = icon.IsEmpty() ? nil : icon.ToNSImage();

  std::u16string secondary_label = model->GetSecondaryLabelAt(index);
  if (@available(macOS 14.4, *)) {
    if ([item respondsToSelector:@selector(setSubtitle:)]) {
      NSString* subtitle = secondary_label.empty()
                               ? @""
                               : base::SysUTF16ToNSString(secondary_label);
      [item setValue:subtitle forKey:@"subtitle"];
    }
  }

  ui::Accelerator accelerator;
  if (model->GetAcceleratorAtWithParams(index, useDefaultAccelerator_,
                                        &accelerator)) {
    NSString* key = KeyEquivalentFromAccelerator(accelerator);
    item.keyEquivalent = key;
    item.keyEquivalentModifierMask = ModifierMaskFromAccelerator(accelerator);
  } else {
    item.keyEquivalent = @"";
    item.keyEquivalentModifierMask = 0;
  }

  std::u16string role = model->GetRoleAt(index);
  if (role == u"sharemenu") {
    SharingItem sharing_item;
    BOOL has_sharing_item = model->GetSharingItemAt(index, &sharing_item);
    item.submenu = has_sharing_item ? [self createShareMenuForItem:sharing_item]
                                    : MakeEmptySubmenu();
  } else if (model->GetTypeAt(index) ==
             lynxtron::LynxtronMenuModel::TYPE_SUBMENU) {
    lynxtron::LynxtronMenuModel* submenu_model =
        model->GetSubmenuModelAt(index);
    if (submenu_model) {
      NSMenu* submenu = item.submenu;
      if (!submenu) {
        submenu = [self menuFromModel:submenu_model];
        item.submenu = submenu;
      } else {
        [self populateMenu:submenu withModel:submenu_model];
        submenu.delegate = self;
      }
      submenu.title = item.title;
    }
  }
}

- (lynxtron::LynxtronMenuModel*)modelForMenu:(NSMenu*)menu {
  if (menu == menu_) {
    return model_.get();
  }

  NSMenu* parent_menu = menu.supermenu;
  if (!parent_menu) {
    return model_.get();
  }

  NSInteger parent_index = [parent_menu indexOfItemWithSubmenu:menu];
  if (parent_index < 0) {
    return model_.get();
  }

  NSMenuItem* parent_item = [parent_menu itemAtIndex:parent_index];
  lynxtron::LynxtronMenuModel* parent_model =
      [WeakPtrToLynxtronMenuModelAsNSObject
          getFrom:parent_item.representedObject];
  if (!parent_model) {
    return nullptr;
  }
  return parent_model->GetSubmenuModelAt(static_cast<size_t>(parent_item.tag));
}

- (void)finalizeTrackingIfNeeded {
  if (openMenuCount_ != 0 || popupCloseCallback.is_null()) {
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(popupCloseCallback));
}

- (void)refreshMenuTree:(NSMenu*)menu {
  for (NSMenuItem* item in menu.itemArray) {
    [self applyStateToMenuItem:item];
    if (item.submenu) {
      [self refreshMenuTree:item.submenu];
    }
  }
}

- (void)addItemToMenu:(NSMenu*)menu
              atIndex:(NSInteger)index
            fromModel:(lynxtron::LynxtronMenuModel*)model {
  [menu insertItem:[self makeMenuItemForIndex:index fromModel:model]
           atIndex:index];
}

- (void)itemSelected:(id)sender {
  NSMenuItem* menuItem = base::apple::ObjCCastStrict<NSMenuItem>(sender);
  lynxtron::LynxtronMenuModel* model =
      [WeakPtrToLynxtronMenuModelAsNSObject getFrom:menuItem.representedObject];
  if (!model) {
    return;
  }
  const size_t modelIndex = static_cast<size_t>(menuItem.tag);
  model->ActivatedAt(modelIndex, EventFlagsFromCurrentEvent());
}

- (NSMenu*)menu {
  if (!menu_) {
    menu_ = [self menuFromModel:model_.get()];
  }
  return menu_;
}

- (BOOL)isMenuOpen {
  return isMenuOpen_;
}

- (void)menuWillOpen:(NSMenu*)menu {
  isMenuOpen_ = YES;

  lynxtron::LynxtronMenuModel* model = [self modelForMenu:menu];
  if (model) {
    [self populateMenu:menu withModel:model];
    menu.delegate = self;
    model->MenuWillShow();
  }
  [self refreshMenuTree:menu];

  if (pendingClose_) {
    pendingClose_ = NO;
    [self cancel];
  }
}

- (void)menuDidClose:(NSMenu*)menu {
  if (!isMenuOpen_) {
    return;
  }

  bool has_close_cb = !popupCloseCallback.is_null();
  bool should_emit_close = true;
  if (menu != menu_) {
    should_emit_close = !has_close_cb && menu.supermenu == menu_;
  }

  [self refreshMenuTree:menu];

  if (!should_emit_close) {
    return;
  }

  isClosing_ = NO;
  isMenuOpen_ = NO;

  lynxtron::LynxtronMenuModel* model = [self modelForMenu:menu];
  if (model) {
    model->MenuWillClose();
  }

  if (has_close_cb) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(popupCloseCallback));
  }
}

@end
