// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "tray_icon_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/current_thread.h"
#include "shell/common/global_thread.h"
#include "shell/ui/cocoa/lynxtron_menu_controller.h"
#include "shell/ui/gfx/image/image.h"

@interface LynxtronTrayIconTarget : NSObject
@property(nonatomic, assign) lynxtron::TrayIconMac* trayIcon;
- (void)handleAction:(id)sender;
- (void)mouseEntered:(NSEvent*)event;
- (void)mouseExited:(NSEvent*)event;
- (void)mouseMoved:(NSEvent*)event;
@end

@implementation LynxtronTrayIconTarget
- (void)handleAction:(id)sender {
  if (self.trayIcon) {
    self.trayIcon->HandleAction();
  }
}

- (void)mouseEntered:(NSEvent*)event {
  if (!self.trayIcon) {
    return;
  }
  NSPoint location = [NSEvent mouseLocation];
  self.trayIcon->HandleMouseEntered(lynxtron::TrayPoint{
      static_cast<int>(location.x), static_cast<int>(location.y)});
}

- (void)mouseExited:(NSEvent*)event {
  if (!self.trayIcon) {
    return;
  }
  NSPoint location = [NSEvent mouseLocation];
  self.trayIcon->HandleMouseExited(lynxtron::TrayPoint{
      static_cast<int>(location.x), static_cast<int>(location.y)});
}

- (void)mouseMoved:(NSEvent*)event {
  if (!self.trayIcon) {
    return;
  }
  NSPoint location = [NSEvent mouseLocation];
  self.trayIcon->HandleMouseMoved(lynxtron::TrayPoint{
      static_cast<int>(location.x), static_cast<int>(location.y)});
}
@end

namespace lynxtron {

TrayIconMac::TrayIconMac(TrayIconObserver* observer, const std::string& guid)
    : observer_(observer), guid_(guid) {
  NSStatusItem* status_item = [[NSStatusBar systemStatusBar]
      statusItemWithLength:NSSquareStatusItemLength];
  status_item_ = (__bridge_retained void*)status_item;
  if ([status_item respondsToSelector:@selector(setAutosaveName:)] &&
      !guid_.empty()) {
    status_item.autosaveName = base::SysUTF8ToNSString(guid_);
  }

  NSStatusBarButton* button = status_item.button;
  LynxtronTrayIconTarget* target = [[LynxtronTrayIconTarget alloc] init];
  target.trayIcon = this;
  target_ = (__bridge_retained void*)target;
  [button setTarget:target];
  [button setAction:@selector(handleAction:)];
  [button sendActionOn:(NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp |
                        NSEventMaskRightMouseDown | NSEventMaskRightMouseUp |
                        NSEventMaskOtherMouseDown | NSEventMaskOtherMouseUp)];

  NSTrackingArea* tracking_area = [[NSTrackingArea alloc]
      initWithRect:button.bounds
           options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                    NSTrackingActiveAlways | NSTrackingInVisibleRect)
             owner:target
          userInfo:nil];
  tracking_area_ = (__bridge_retained void*)tracking_area;
  [button addTrackingArea:tracking_area];
}

TrayIconMac::~TrayIconMac() {
  weak_factory_.InvalidateWeakPtrs();
  NSStatusItem* status_item = (__bridge NSStatusItem*)status_item_;
  if (status_item) {
    NSStatusBarButton* button = status_item.button;
    NSTrackingArea* tracking_area = (__bridge NSTrackingArea*)tracking_area_;
    if (tracking_area) {
      [button removeTrackingArea:tracking_area];
    }
    [[NSStatusBar systemStatusBar] removeStatusItem:status_item];
  }
  if (tracking_area_) {
    CFBridgingRelease(tracking_area_);
    tracking_area_ = nullptr;
  }
  LynxtronTrayIconTarget* target = (__bridge LynxtronTrayIconTarget*)target_;
  if (target) {
    target.trayIcon = nullptr;
  }
  if (target_) {
    CFBridgingRelease(target_);
    target_ = nullptr;
  }
  ResetPopupMenuController();
  if (status_item_) {
    CFBridgingRelease(status_item_);
    status_item_ = nullptr;
  }
}

void TrayIconMac::SetImage(const gfx::Image& image) {
  NSStatusItem* status_item = (__bridge NSStatusItem*)status_item_;
  status_item.button.image = image.AsNSImage();
}

void TrayIconMac::SetPressedImage(const gfx::Image& image) {
  NSStatusItem* status_item = (__bridge NSStatusItem*)status_item_;
  status_item.button.alternateImage = image.AsNSImage();
}

void TrayIconMac::SetToolTip(const std::string& tool_tip) {
  NSStatusItem* status_item = (__bridge NSStatusItem*)status_item_;
  status_item.button.toolTip = base::SysUTF8ToNSString(tool_tip);
}

void TrayIconMac::SetTitle(const std::string& title,
                           const std::string& font_type) {
  NSStatusItem* status_item = (__bridge NSStatusItem*)status_item_;
  NSString* ns_title = base::SysUTF8ToNSString(title);
  NSMutableAttributedString* attributed_title =
      [[NSMutableAttributedString alloc] initWithString:ns_title];

  CGFloat existing_size = status_item.button.font.pointSize;
  if (font_type == "monospaced") {
    NSDictionary* attributes = @{
      NSFontAttributeName :
          [NSFont monospacedSystemFontOfSize:existing_size
                                      weight:NSFontWeightRegular]
    };
    [attributed_title addAttributes:attributes
                              range:NSMakeRange(0, attributed_title.length)];
  } else if (font_type == "monospacedDigit") {
    NSDictionary* attributes = @{
      NSFontAttributeName :
          [NSFont monospacedDigitSystemFontOfSize:existing_size
                                           weight:NSFontWeightRegular]
    };
    [attributed_title addAttributes:attributes
                              range:NSMakeRange(0, attributed_title.length)];
  }

  status_item.button.title = ns_title;
  status_item.button.attributedTitle = attributed_title;
  status_item.button.imagePosition =
      ns_title.length > 0 ? NSImageLeft : NSImageOnly;
}

std::string TrayIconMac::GetTitle() const {
  NSStatusItem* status_item = (__bridge NSStatusItem*)status_item_;
  return base::SysNSStringToUTF8(status_item.button.title);
}

void TrayIconMac::SetIgnoreDoubleClickEvents(bool ignore) {
  ignore_double_click_events_ = ignore;
}

bool TrayIconMac::GetIgnoreDoubleClickEvents() const {
  return ignore_double_click_events_;
}

void TrayIconMac::SetContextMenu(LynxtronMenuModel* menu) {
  menu_model_ = menu;
}

void TrayIconMac::PopUpContextMenu(LynxtronMenuModel* menu,
                                   const TrayPoint& position) {
  LynxtronMenuModel* menu_to_use = menu ? menu : menu_model_;
  if (!menu_to_use) {
    return;
  }
  pending_close_ = false;
  uint64_t popup_id = ++popup_request_id_;
  GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&TrayIconMac::PopUpContextMenuOnUI,
                                weak_factory_.GetWeakPtr(), popup_id,
                                menu_to_use, position));
}

void TrayIconMac::CloseContextMenu() {
  pending_close_ = true;
  GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&TrayIconMac::CloseContextMenuOnUI,
                                weak_factory_.GetWeakPtr()));
}

void TrayIconMac::PopUpContextMenuOnUI(uint64_t popup_id,
                                       LynxtronMenuModel* menu,
                                       const TrayPoint& position) {
  (void)position;
  if (!status_item_ || !menu || popup_id != popup_request_id_) {
    return;
  }
  if (pending_close_) {
    pending_close_ = false;
    return;
  }

  LynxtronMenuController* active_controller =
      (__bridge LynxtronMenuController*)popup_menu_controller_;
  if (active_controller && [active_controller isMenuOpen]) {
    return;
  }

  ResetPopupMenuController();
  LynxtronMenuController* controller =
      [[LynxtronMenuController alloc] initWithModel:menu
                              useDefaultAccelerator:NO];
  active_popup_id_ = popup_id;
  [controller setPopupCloseCallback:base::BindOnce(&TrayIconMac::OnPopupClosed,
                                                   weak_factory_.GetWeakPtr(),
                                                   popup_id)];
  popup_menu_controller_ = (__bridge_retained void*)controller;

  base::WeakPtr<TrayIconMac> weak_this = weak_factory_.GetWeakPtr();
  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
  NSStatusItem* status_item = (__bridge NSStatusItem*)status_item_;
  [status_item popUpStatusItemMenu:controller.menu];
  if (!weak_this) {
    return;
  }
}

void TrayIconMac::CloseContextMenuOnUI() {
  LynxtronMenuController* controller =
      (__bridge LynxtronMenuController*)popup_menu_controller_;
  if (controller) {
    [controller cancel];
  }
}

void TrayIconMac::OnPopupClosed(uint64_t popup_id) {
  if (popup_id != active_popup_id_) {
    return;
  }
  pending_close_ = false;
  active_popup_id_ = 0;
  ResetPopupMenuController();
}

void TrayIconMac::ResetPopupMenuController() {
  if (!popup_menu_controller_) {
    return;
  }
  CFBridgingRelease(popup_menu_controller_);
  popup_menu_controller_ = nullptr;
}

TrayBounds TrayIconMac::GetBounds() const {
  NSStatusItem* status_item = (__bridge NSStatusItem*)status_item_;
  NSStatusBarButton* button = status_item.button;
  NSRect rect = [button.window convertRectToScreen:button.frame];
  return TrayBounds{
      static_cast<int>(rect.origin.x), static_cast<int>(rect.origin.y),
      static_cast<int>(rect.size.width), static_cast<int>(rect.size.height)};
}

std::string TrayIconMac::GetGUID() const {
  return guid_;
}

void TrayIconMac::HandleAction() {
  if (!observer_) {
    return;
  }
  NSEvent* event = [NSApp currentEvent];
  if (!event) {
    return;
  }
  TrayBounds bounds = GetBounds();
  NSPoint location = [NSEvent mouseLocation];
  TrayPoint position{static_cast<int>(location.x),
                     static_cast<int>(location.y)};

  switch (event.type) {
    case NSEventTypeLeftMouseDown:
      observer_->OnMouseDown(position);
      return;
    case NSEventTypeLeftMouseUp: {
      observer_->OnMouseUp(position);
      if (event.clickCount > 1 && !ignore_double_click_events_) {
        observer_->OnDoubleClick(bounds);
        return;
      }
      observer_->OnClick(bounds, position);
      return;
    }
    case NSEventTypeRightMouseDown:
      observer_->OnMouseDown(position);
      return;
    case NSEventTypeRightMouseUp:
      observer_->OnMouseUp(position);
      observer_->OnRightClick(bounds);
      if (menu_model_) {
        PopUpContextMenu(menu_model_, position);
      }
      return;
    case NSEventTypeOtherMouseDown:
      observer_->OnMouseDown(position);
      return;
    case NSEventTypeOtherMouseUp:
      observer_->OnMouseUp(position);
      return;
    default:
      return;
  }
}

void TrayIconMac::HandleMouseEntered(const TrayPoint& position) {
  if (observer_) {
    observer_->OnMouseEnter(position);
  }
}

void TrayIconMac::HandleMouseExited(const TrayPoint& position) {
  if (observer_) {
    observer_->OnMouseLeave(position);
  }
}

void TrayIconMac::HandleMouseMoved(const TrayPoint& position) {
  if (observer_) {
    observer_->OnMouseMove(position);
  }
}

}  // namespace lynxtron
