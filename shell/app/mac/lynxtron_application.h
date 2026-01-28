// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_APP_MAC_LYNXTRON_APPLICATION_H_
#define LYNXTRON_SHELL_APP_MAC_LYNXTRON_APPLICATION_H_

#import <AVFoundation/AVFoundation.h>
#import <LocalAuthentication/LocalAuthentication.h>

#include "base/functional/callback.h"
#include "base/mac/scoped_sending_event.h"

@interface LynxtronApplication : NSApplication <CrAppProtocol,
                                                CrAppControlProtocol,
                                                NSUserActivityDelegate> {
 @private
  BOOL handlingSendEvent_;
  NSUserActivity* __strong currentActivity_;
  NSCondition* handoffLock_;
  BOOL updateReceived_;
  BOOL userStoppedShutdown_;
  base::RepeatingCallback<bool()> shouldShutdown_;
}

+ (LynxtronApplication*)sharedApplication;

- (void)setShutdownHandler:(base::RepeatingCallback<bool()>)handler;
- (void)registerURLHandler;

// Called when macOS itself is shutting down.
- (void)willPowerOff:(NSNotification*)notify;

// CrAppProtocol:
- (BOOL)isHandlingSendEvent;

// CrAppControlProtocol:
- (void)setHandlingSendEvent:(BOOL)handlingSendEvent;

- (NSUserActivity*)getCurrentActivity;
- (void)setCurrentActivity:(NSString*)type
              withUserInfo:(NSDictionary*)userInfo
            withWebpageURL:(NSURL*)webpageURL;
- (void)invalidateCurrentActivity;
- (void)resignCurrentActivity;
- (void)updateCurrentActivity:(NSString*)type
                 withUserInfo:(NSDictionary*)userInfo;

@end

#endif  // LYNXTRON_SHELL_APP_MAC_LYNXTRON_APPLICATION_H_
