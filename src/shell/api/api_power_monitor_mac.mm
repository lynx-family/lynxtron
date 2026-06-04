// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_power_monitor.h"

#import <Cocoa/Cocoa.h>

#include <vector>

using LynxtronPowerMonitor = lynxtron::api::PowerMonitor;

@interface LynxtronPowerMonitorMacObserver : NSObject {
 @private
  std::vector<LynxtronPowerMonitor*> emitters_;
}

- (void)addEmitter:(LynxtronPowerMonitor*)monitor;
- (void)removeEmitter:(LynxtronPowerMonitor*)monitor;

@end

@implementation LynxtronPowerMonitorMacObserver

- (id)init {
  if ((self = [super init])) {
    NSDistributedNotificationCenter* distributed_center =
        [NSDistributedNotificationCenter defaultCenter];
    [distributed_center addObserver:self
                           selector:@selector(onScreenLocked:)
                               name:@"com.apple.screenIsLocked"
                             object:nil];
    [distributed_center addObserver:self
                           selector:@selector(onScreenUnlocked:)
                               name:@"com.apple.screenIsUnlocked"
                             object:nil];

    NSNotificationCenter* workspace_center =
        [[NSWorkspace sharedWorkspace] notificationCenter];
    [workspace_center addObserver:self
                         selector:@selector(onUserDidBecomeActive:)
                             name:NSWorkspaceSessionDidBecomeActiveNotification
                           object:nil];
    [workspace_center addObserver:self
                         selector:@selector(onUserDidResignActive:)
                             name:NSWorkspaceSessionDidResignActiveNotification
                           object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
  [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
}

- (void)addEmitter:(LynxtronPowerMonitor*)monitor {
  emitters_.push_back(monitor);
}

- (void)removeEmitter:(LynxtronPowerMonitor*)monitor {
  emitters_.erase(std::remove(emitters_.begin(), emitters_.end(), monitor),
                  emitters_.end());
}

- (void)onScreenLocked:(NSNotification*)notification {
  for (auto* emitter : emitters_) {
    emitter->Emit("lock-screen");
  }
}

- (void)onScreenUnlocked:(NSNotification*)notification {
  for (auto* emitter : emitters_) {
    emitter->Emit("unlock-screen");
  }
}

- (void)onUserDidBecomeActive:(NSNotification*)notification {
  for (auto* emitter : emitters_) {
    emitter->Emit("user-did-become-active");
  }
}

- (void)onUserDidResignActive:(NSNotification*)notification {
  for (auto* emitter : emitters_) {
    emitter->Emit("user-did-resign-active");
  }
}

@end

namespace lynxtron::api {

namespace {

LynxtronPowerMonitorMacObserver* g_power_monitor_mac_observer = nil;

}  // namespace

void PowerMonitor::InitPlatformSpecificMonitors() {
  if (!g_power_monitor_mac_observer) {
    g_power_monitor_mac_observer =
        [[LynxtronPowerMonitorMacObserver alloc] init];
  }

  [g_power_monitor_mac_observer addEmitter:this];
}

void PowerMonitor::ShutdownPlatformSpecificMonitors() {
  [g_power_monitor_mac_observer removeEmitter:this];
}

}  // namespace lynxtron::api
