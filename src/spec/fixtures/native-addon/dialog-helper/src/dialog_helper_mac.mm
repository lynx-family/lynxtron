// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "dialog_helper.h"

#import <Cocoa/Cocoa.h>

namespace {

NSWindow* GetNSWindowFromHandle(char* handle, size_t size) {
  if (size != sizeof(void*)) {
    return nil;
  }

  void* raw = *reinterpret_cast<void**>(handle);
  NSView* view = (__bridge NSView*)raw;
  if (!view || ![view isKindOfClass:[NSView class]]) {
    return nil;
  }

  return [view window];
}

NSMutableArray<NSButton*>* CollectPushButtons(NSView* root_view) {
  NSMutableArray<NSButton*>* buttons = [NSMutableArray array];
  NSMutableArray<NSView*>* stack = [NSMutableArray arrayWithObject:root_view];

  while ([stack count] > 0) {
    NSView* current = [stack lastObject];
    [stack removeLastObject];

    if ([current isKindOfClass:[NSButton class]]) {
      NSButton* button = static_cast<NSButton*>(current);
      if ([button bezelStyle] == NSBezelStyleRounded ||
          [button bezelStyle] == NSBezelStyleRegularSquare) {
        [buttons addObject:button];
      }
    }

    for (NSView* subview in [current subviews]) {
      [stack addObject:subview];
    }
  }

  [buttons sortUsingComparator:^NSComparisonResult(NSButton* a, NSButton* b) {
    if ([a tag] < [b tag]) {
      return NSOrderedAscending;
    }
    if ([a tag] > [b tag]) {
      return NSOrderedDescending;
    }
    return NSOrderedSame;
  }];

  return buttons;
}

NSButton* FindCheckbox(NSView* root_view) {
  NSMutableArray<NSView*>* stack = [NSMutableArray arrayWithObject:root_view];

  while ([stack count] > 0) {
    NSView* current = [stack lastObject];
    [stack removeLastObject];

    if ([current isKindOfClass:[NSButton class]]) {
      NSButton* button = static_cast<NSButton*>(current);
      if (![button isBordered] && [[button title] length] > 0) {
        return button;
      }
    }

    for (NSView* subview in [current subviews]) {
      [stack addObject:subview];
    }
  }

  return nil;
}

}  // namespace

namespace dialog_helper {

DialogInfo GetDialogInfo(char* handle, size_t size) {
  DialogInfo info;
  info.type = "none";

  NSWindow* window = GetNSWindowFromHandle(handle, size);
  if (!window) {
    return info;
  }

  NSWindow* sheet = [window attachedSheet];
  if (!sheet) {
    return info;
  }

  if ([sheet isKindOfClass:[NSOpenPanel class]]) {
    info.type = "open-dialog";
    NSOpenPanel* panel = static_cast<NSOpenPanel*>(sheet);
    info.message = [[panel title] UTF8String] ?: "";
    info.prompt = [[panel prompt] UTF8String] ?: "";
    info.panel_message = [[panel message] UTF8String] ?: "";
    if ([panel directoryURL]) {
      info.directory = [[[panel directoryURL] path] UTF8String] ?: "";
    }
    info.can_choose_files = [panel canChooseFiles];
    info.can_choose_directories = [panel canChooseDirectories];
    info.allows_multiple_selection = [panel allowsMultipleSelection];
    info.shows_hidden_files = [panel showsHiddenFiles];
    info.resolves_aliases = [panel resolvesAliases];
    info.treats_packages_as_directories =
        [panel treatsFilePackagesAsDirectories];
    info.can_create_directories = [panel canCreateDirectories];
    return info;
  }

  if ([sheet isKindOfClass:[NSSavePanel class]]) {
    info.type = "save-dialog";
    NSSavePanel* panel = static_cast<NSSavePanel*>(sheet);
    info.message = [[panel title] UTF8String] ?: "";
    info.prompt = [[panel prompt] UTF8String] ?: "";
    info.panel_message = [[panel message] UTF8String] ?: "";
    if ([panel directoryURL]) {
      info.directory = [[[panel directoryURL] path] UTF8String] ?: "";
    }
    info.name_field_label = [[panel nameFieldLabel] UTF8String] ?: "";
    info.name_field_value = [[panel nameFieldStringValue] UTF8String] ?: "";
    info.shows_tag_field = [panel showsTagField];
    info.shows_hidden_files = [panel showsHiddenFiles];
    info.treats_packages_as_directories =
        [panel treatsFilePackagesAsDirectories];
    info.can_create_directories = [panel canCreateDirectories];
    return info;
  }

  NSView* content_view = [sheet contentView];
  NSMutableArray<NSButton*>* buttons = CollectPushButtons(content_view);
  if ([buttons count] == 0) {
    return info;
  }

  info.type = "message-box";

  std::string buttons_json = "[";
  for (NSUInteger i = 0; i < [buttons count]; ++i) {
    if (i > 0) {
      buttons_json += ",";
    }
    buttons_json += "\"";
    NSString* title = [[buttons objectAtIndex:i] title];
    buttons_json += [title UTF8String] ?: "";
    buttons_json += "\"";
  }
  buttons_json += "]";
  info.buttons = buttons_json;

  int text_field_index = 0;
  for (NSView* subview in [content_view subviews]) {
    if ([subview isKindOfClass:[NSTextField class]]) {
      NSTextField* field = static_cast<NSTextField*>(subview);
      if (![field isEditable] && [[field stringValue] length] > 0) {
        if (text_field_index == 0) {
          info.message = [[field stringValue] UTF8String];
        } else if (text_field_index == 1) {
          info.detail = [[field stringValue] UTF8String];
        }
        ++text_field_index;
      }
    }
  }

  if (NSButton* checkbox = FindCheckbox(content_view)) {
    info.checkbox_label = [[checkbox title] UTF8String] ?: "";
    info.checkbox_checked = [checkbox state] == NSControlStateValueOn;
  }

  return info;
}

bool ClickMessageBoxButton(char* handle, size_t size, int button_index) {
  NSWindow* window = GetNSWindowFromHandle(handle, size);
  if (!window) {
    return false;
  }

  NSWindow* sheet = [window attachedSheet];
  if (!sheet) {
    return false;
  }

  NSMutableArray<NSButton*>* buttons = CollectPushButtons([sheet contentView]);
  if (button_index < 0 || button_index >= static_cast<int>([buttons count])) {
    return false;
  }

  [[buttons objectAtIndex:button_index] performClick:nil];
  return true;
}

bool ClickCheckbox(char* handle, size_t size) {
  NSWindow* window = GetNSWindowFromHandle(handle, size);
  if (!window) {
    return false;
  }

  NSWindow* sheet = [window attachedSheet];
  if (!sheet) {
    return false;
  }

  NSButton* checkbox = FindCheckbox([sheet contentView]);
  if (!checkbox) {
    return false;
  }

  [checkbox performClick:nil];
  return true;
}

bool CancelFileDialog(char* handle, size_t size) {
  NSWindow* window = GetNSWindowFromHandle(handle, size);
  if (!window) {
    return false;
  }

  NSWindow* sheet = [window attachedSheet];
  if (!sheet) {
    return false;
  }

  if ([sheet isKindOfClass:[NSSavePanel class]]) {
    [static_cast<NSSavePanel*>(sheet) cancel:nil];
    return true;
  }

  [NSApp endSheet:sheet returnCode:NSModalResponseCancel];
  return true;
}

bool AcceptFileDialog(char* handle, size_t size, const std::string& filename) {
  NSWindow* window = GetNSWindowFromHandle(handle, size);
  if (!window) {
    return false;
  }

  NSWindow* sheet = [window attachedSheet];
  if (!sheet || ![sheet isKindOfClass:[NSSavePanel class]]) {
    return false;
  }

  NSSavePanel* panel = static_cast<NSSavePanel*>(sheet);
  if (!filename.empty()) {
    NSString* name = [NSString stringWithUTF8String:filename.c_str()];
    [panel setNameFieldStringValue:name];
    [sheet makeFirstResponder:nil];
  }

  NSMutableArray<NSView*>* stack =
      [NSMutableArray arrayWithObject:[sheet contentView]];
  while ([stack count] > 0) {
    NSView* current = [stack lastObject];
    [stack removeLastObject];

    if ([current isKindOfClass:[NSButton class]]) {
      NSButton* button = static_cast<NSButton*>(current);
      if ([[button keyEquivalent] isEqualToString:@"\r"]) {
        [button performClick:nil];
        return true;
      }
    }

    for (NSView* subview in [current subviews]) {
      [stack addObject:subview];
    }
  }

  [NSApp endSheet:sheet returnCode:NSModalResponseOK];
  return true;
}

}  // namespace dialog_helper
