// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import "shell/app/mac/lynxtron_application.h"

#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/observer_list.h"
#include "base/strings/sys_string_conversions.h"
// #include "electron/buildflags/buildflags.h"

#include "shell/app/application.h"
#include "shell/app/mac/dict_util.h"
#import "shell/app/mac/lynxtron_application_delegate.h"

@implementation LynxtronApplication

+ (LynxtronApplication*)sharedApplication {
  return (LynxtronApplication*)[super sharedApplication];
}

- (void)willPowerOff:(NSNotification*)notify {
  userStoppedShutdown_ = shouldShutdown_ && !shouldShutdown_.Run();
}

- (void)terminate:(id)sender {
  // User will call Quit later.
  if (userStoppedShutdown_) {
    return;
  }

  // We simply try to close the browser, which in turn will try to close the
  // windows. Termination can proceed if all windows are closed or window close
  // can be cancelled which will abort termination.
  lynxtron::Application::Get()->Quit();
}

- (void)setShutdownHandler:(base::RepeatingCallback<bool()>)handler {
  shouldShutdown_ = std::move(handler);
}

- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)sendEvent:(NSEvent*)event {
  base::AutoReset<BOOL> scoper(&handlingSendEvent_, YES);
  [super sendEvent:event];
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (void)setCurrentActivity:(NSString*)type
              withUserInfo:(NSDictionary*)userInfo
            withWebpageURL:(NSURL*)webpageURL {
  currentActivity_ = [[NSUserActivity alloc] initWithActivityType:type];
  [currentActivity_ setUserInfo:userInfo];
  [currentActivity_ setWebpageURL:webpageURL];
  [currentActivity_ setDelegate:self];
  [currentActivity_ becomeCurrent];
  [currentActivity_ setNeedsSave:YES];
}

- (NSUserActivity*)getCurrentActivity {
  return currentActivity_;
}

- (void)invalidateCurrentActivity {
  if (currentActivity_) {
    [currentActivity_ invalidate];
    currentActivity_ = nil;
  }
}

- (void)resignCurrentActivity {
  if (currentActivity_) {
    [currentActivity_ resignCurrent];
  }
}

- (void)updateCurrentActivity:(NSString*)type
                 withUserInfo:(NSDictionary*)userInfo {
  if (currentActivity_) {
    [currentActivity_ addUserInfoEntriesFromDictionary:userInfo];
  }

  [handoffLock_ lock];
  updateReceived_ = YES;
  [handoffLock_ signal];
  [handoffLock_ unlock];
}

- (void)userActivityWillSave:(NSUserActivity*)userActivity {
  __block BOOL shouldWait = NO;

  if (shouldWait) {
    [handoffLock_ lock];
    updateReceived_ = NO;
    while (!updateReceived_) {
      BOOL isSignaled =
          [handoffLock_ waitUntilDate:[NSDate dateWithTimeIntervalSinceNow:1]];
      if (!isSignaled) {
        break;
      }
    }
    [handoffLock_ unlock];
  }

  [userActivity setNeedsSave:YES];
}

- (void)userActivityWasContinued:(NSUserActivity*)userActivity {
  [userActivity setNeedsSave:YES];
}

- (void)registerURLHandler {
  [[NSAppleEventManager sharedAppleEventManager]
      setEventHandler:self
          andSelector:@selector(handleURLEvent:withReplyEvent:)
        forEventClass:kInternetEventClass
           andEventID:kAEGetURL];

  handoffLock_ = [NSCondition new];
}

- (void)handleURLEvent:(NSAppleEventDescriptor*)event
        withReplyEvent:(NSAppleEventDescriptor*)replyEvent {
  NSString* url =
      [[event paramDescriptorForKeyword:keyDirectObject] stringValue];
  lynxtron::Application::Get()->OpenURL(base::SysNSStringToUTF8(url));
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  // Undocumented attribute that screen reader related functionality
  // sets when running.

  return [super accessibilitySetValue:value forAttribute:attribute];
}

- (void)orderFrontStandardAboutPanel:(id)sender {
  lynxtron::Application::Get()->ShowAboutPanel();
}

@end
