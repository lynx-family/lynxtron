// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "tray_icon_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"
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
  if (menu_controller_) {
    CFBridgingRelease(menu_controller_);
    menu_controller_ = nullptr;
  }
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

void TrayIconMac::SetTitle(const std::string& title) {
  NSStatusItem* status_item = (__bridge NSStatusItem*)status_item_;
  status_item.button.title = base::SysUTF8ToNSString(title);
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
  if (!menu) {
    if (menu_controller_) {
      CFBridgingRelease(menu_controller_);
      menu_controller_ = nullptr;
    }
    menu_controller_ = nullptr;
    return;
  }
  if (menu_controller_) {
    CFBridgingRelease(menu_controller_);
    menu_controller_ = nullptr;
  }
  LynxtronMenuController* controller =
      [[LynxtronMenuController alloc] initWithModel:menu
                              useDefaultAccelerator:NO];
  menu_controller_ = (__bridge_retained void*)controller;
}

void TrayIconMac::PopUpContextMenu(LynxtronMenuModel* menu,
                                   const TrayPoint& position) {
  LynxtronMenuModel* menu_to_use = menu ? menu : menu_model_;
  if (!menu_to_use) {
    return;
  }
  if (!menu_controller_ || menu_to_use != menu_model_) {
    menu_model_ = menu_to_use;
    if (menu_controller_) {
      CFBridgingRelease(menu_controller_);
      menu_controller_ = nullptr;
    }
    LynxtronMenuController* controller =
        [[LynxtronMenuController alloc] initWithModel:menu_to_use
                                useDefaultAccelerator:NO];
    menu_controller_ = (__bridge_retained void*)controller;
  }
  NSStatusItem* status_item = (__bridge NSStatusItem*)status_item_;
  LynxtronMenuController* controller =
      (__bridge LynxtronMenuController*)menu_controller_;
  [status_item popUpStatusItemMenu:controller.menu];
}

void TrayIconMac::CloseContextMenu() {
  LynxtronMenuController* controller =
      (__bridge LynxtronMenuController*)menu_controller_;
  if (controller) {
    [controller cancel];
  }
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
