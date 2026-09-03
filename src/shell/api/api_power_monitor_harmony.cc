// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_power_monitor.h"

#include <mutex>
#include <vector>

#include "base/functional/bind.h"


namespace lynxtron::api {
namespace {

std::mutex g_monitors_lock;
std::vector<PowerMonitor*> g_monitors;

void NotifyMonitors(bool locked) {
  std::lock_guard<std::mutex> lock(g_monitors_lock);
  for (auto* monitor : g_monitors) {
    if (locked) {
      monitor->QueueLockScreen();
    } else {
      monitor->QueueUnlockScreen();
    }
  }
}

}  // namespace

void PowerMonitor::InitPlatformSpecificMonitors() {
  task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  weak_this_ = weak_factory_.GetWeakPtr();
  std::lock_guard<std::mutex> lock(g_monitors_lock);
  g_monitors.push_back(this);
}

void PowerMonitor::ShutdownPlatformSpecificMonitors() {
  std::lock_guard<std::mutex> lock(g_monitors_lock);
  std::erase(g_monitors, this);
}

void PowerMonitor::EmitLockScreen() {
  Emit("lock-screen");
}

void PowerMonitor::EmitUnlockScreen() {
  Emit("unlock-screen");
}

void PowerMonitor::QueueLockScreen() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PowerMonitor::EmitLockScreen, weak_this_));
}

void PowerMonitor::QueueUnlockScreen() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PowerMonitor::EmitUnlockScreen,
                                weak_this_));
}

extern "C" __attribute__((visibility("default")))
void LynxtronPowerMonitorNotifyLockScreen() {
  NotifyMonitors(true);
}

extern "C" __attribute__((visibility("default")))
void LynxtronPowerMonitorNotifyUnlockScreen() {
  NotifyMonitors(false);
}

}  // namespace lynxtron::api