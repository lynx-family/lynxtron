// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#import "shell/api/api_menu_mac.h"

#include <string>
#include <utility>

#include "base/mac/scoped_sending_event.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "shell/api/api_base_window.h"
#include "shell/app/native_window.h"
#include "shell/common/node_includes.h"
#include "v8/include/cppgc/allocation.h"

namespace {

static NSMenu* __strong applicationMenu_;

ui::Accelerator GetAcceleratorFromKeyEquivalentAndModifierMask(
    NSString* key_equivalent,
    NSUInteger modifier_mask) {
  int modifiers = 0;
  if (modifier_mask & NSEventModifierFlagShift) {
    modifiers |= ui::Accelerator::kShift;
  }
  if (modifier_mask & NSEventModifierFlagControl) {
    modifiers |= ui::Accelerator::kCtrl;
  }
  if (modifier_mask & NSEventModifierFlagOption) {
    modifiers |= ui::Accelerator::kAlt;
  }
  if (modifier_mask & NSEventModifierFlagCommand) {
    modifiers |= ui::Accelerator::kCmd;
  }
  return ui::Accelerator(base::SysNSStringToUTF8(key_equivalent), modifiers);
}

}  // namespace

namespace lynxtron::api {

MenuMac::MenuMac(gin::Arguments* args) : Menu{args} {}

MenuMac::~MenuMac() {
  RemoveModelObserver();
}

void MenuMac::PopupAt(BaseWindow* window,
                      int x,
                      int y,
                      int positioning_item,
                      base::OnceClosure callback) {
  if (!window || !window->window()) {
    return;
  }

  base::OnceClosure callback_with_ref = BindSelfToClosure(std::move(callback));

  auto popup =
      base::BindOnce(&MenuMac::PopupOnUI, weak_factory_.GetWeakPtr(),
                     window->window()->GetWeakPtr(), window->weak_map_id(), x,
                     y, positioning_item, std::move(callback_with_ref));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(popup));
}

v8::Local<v8::Value> Menu::GetUserAcceleratorAt(int command_id) const {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  if (![NSMenuItem usesUserKeyEquivalents]) {
    return v8::Null(isolate);
  }

  auto controller = [[LynxtronMenuController alloc] initWithModel:model()
                                            useDefaultAccelerator:NO];

  auto command_index = model_->GetIndexOfCommandId(command_id);
  if (!command_index) {
    return v8::Null(isolate);
  }

  NSMenuItem* item = [controller makeMenuItemForIndex:*command_index
                                            fromModel:model()];
  if ([[item userKeyEquivalent] length] == 0) {
    return v8::Null(isolate);
  }

  NSString* user_key_equivalent = [item keyEquivalent];
  NSUInteger user_modifier_mask = [item keyEquivalentModifierMask];
  ui::Accelerator accelerator = GetAcceleratorFromKeyEquivalentAndModifierMask(
      user_key_equivalent, user_modifier_mask);

  return gin::ConvertToV8(isolate, accelerator.GetShortcutText());
}

void MenuMac::PopupOnUI(const base::WeakPtr<NativeWindow>& native_window,
                        int32_t window_id,
                        int x,
                        int y,
                        int positioning_item,
                        base::OnceClosure callback) {
  if (!native_window) {
    return;
  }
  NSWindow* nswindow = native_window->GetNativeWindow().GetNativeNSWindow();

  base::OnceClosure close_callback =
      base::BindOnce(&MenuMac::OnClosed, weak_factory_.GetWeakPtr(), window_id,
                     std::move(callback));
  popup_controllers_[window_id] =
      [[LynxtronMenuController alloc] initWithModel:model()
                              useDefaultAccelerator:NO];
  NSMenu* menu = [popup_controllers_[window_id] menu];
  NSView* view = [nswindow contentView];

  NSMenuItem* item = nil;
  if (positioning_item < [menu numberOfItems] && positioning_item >= 0) {
    item = [menu itemAtIndex:positioning_item];
  }

  NSPoint position;
  if (x == -1 || y == -1) {
    position = [view convertPoint:[nswindow mouseLocationOutsideOfEventStream]
                         fromView:nil];
  } else {
    position = NSMakePoint(x, [view frame].size.height - y);
  }

  if (!item) {
    CGFloat windowBottom = CGRectGetMinY([view window].frame);
    CGFloat lowestMenuPoint = windowBottom + position.y - [menu size].height;
    CGFloat screenBottom = CGRectGetMinY([view window].screen.visibleFrame);
    CGFloat distanceFromBottom = lowestMenuPoint - screenBottom;
    if (distanceFromBottom < 0) {
      position.y = position.y - distanceFromBottom + 4;
    }
  }

  CGFloat windowLeft = CGRectGetMinX([view window].frame);
  CGFloat rightmostMenuPoint = windowLeft + position.x + [menu size].width;
  CGFloat screenRight = CGRectGetMaxX([view window].screen.visibleFrame);
  if (rightmostMenuPoint > screenRight) {
    position.x = position.x - [menu size].width;
  }

  [popup_controllers_[window_id]
      setPopupCloseCallback:std::move(close_callback)];

  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
  base::mac::ScopedSendingEvent sendingEventScoper;
  [menu popUpMenuPositioningItem:item atLocation:position inView:view];
}

void MenuMac::ClosePopupAt(int32_t window_id) {
  auto close_popup = base::BindOnce(&MenuMac::ClosePopupOnUI,
                                    weak_factory_.GetWeakPtr(), window_id);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(close_popup));
}

std::u16string MenuMac::GetAcceleratorTextAtForTesting(int index) const {
  LynxtronMenuController* controller =
      [[LynxtronMenuController alloc] initWithModel:model()
                              useDefaultAccelerator:NO];
  NSMenuItem* item = [[controller menu] itemAtIndex:index];
  std::u16string text;
  NSEventModifierFlags modifiers = [item keyEquivalentModifierMask];
  if (modifiers & NSEventModifierFlagControl) {
    text += u"Ctrl";
  }
  if (modifiers & NSEventModifierFlagShift) {
    if (!text.empty()) {
      text += u"+";
    }
    text += u"Shift";
  }
  if (modifiers & NSEventModifierFlagOption) {
    if (!text.empty()) {
      text += u"+";
    }
    text += u"Alt";
  }
  if (modifiers & NSEventModifierFlagCommand) {
    if (!text.empty()) {
      text += u"+";
    }
    text += u"Command";
  }
  if (!text.empty()) {
    text += u"+";
  }
  auto key = base::ToUpperASCII(base::SysNSStringToUTF16([item keyEquivalent]));
  if (key == u"\t") {
    text += u"Tab";
  } else {
    text += key;
  }
  return text;
}

void MenuMac::ClosePopupOnUI(int32_t window_id) {
  auto controller = popup_controllers_.find(window_id);
  if (controller != popup_controllers_.end()) {
    [controller->second cancel];
  } else if (window_id == -1) {
    for (auto it = popup_controllers_.begin();
         it != popup_controllers_.end();) {
      [(it++)->second cancel];
    }
  }
}

void MenuMac::OnClosed(int32_t window_id, base::OnceClosure callback) {
  popup_controllers_.erase(window_id);
  std::move(callback).Run();
}

void Menu::SetApplicationMenu(Menu* base_menu) {
  MenuMac* menu = static_cast<MenuMac*>(base_menu);
  LynxtronMenuController* menu_controller =
      [[LynxtronMenuController alloc] initWithModel:menu->model_.get()
                              useDefaultAccelerator:YES];

  NSRunLoop* currentRunLoop = [NSRunLoop currentRunLoop];
  [currentRunLoop cancelPerformSelector:@selector(setMainMenu:)
                                 target:NSApp
                               argument:applicationMenu_];
  applicationMenu_ = [menu_controller menu];
  [[NSRunLoop currentRunLoop] performSelector:@selector(setMainMenu:)
                                       target:NSApp
                                     argument:applicationMenu_
                                        order:0
                                        modes:@[ NSDefaultRunLoopMode ]];

  menu->menu_controller_ = menu_controller;
}

void Menu::SendActionToFirstResponder(const std::string& action) {
  SEL selector = NSSelectorFromString(base::SysUTF8ToNSString(action));
  [NSApp sendAction:selector to:nil from:[NSApp mainMenu]];
}

Menu* Menu::New(gin::Arguments* args) {
  v8::Isolate* const isolate = args->isolate();
  Menu* const menu = new MenuMac(args);
  gin_helper::CallMethod(isolate, menu, "_init");
  return menu;
}

}  // namespace lynxtron::api
