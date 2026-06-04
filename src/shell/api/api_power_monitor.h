// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_API_POWER_MONITOR_H_
#define LYNXTRON_SHELL_API_API_POWER_MONITOR_H_

#include <memory>
#include <string>

#include "base/power_monitor/power_observer.h"
#include "build/build_config.h"
#include "shell/api/event_emitter_mixin.h"
#include "shell/common/gin_helper/error_thrower.h"
#include "shell/common/gin_helper/handle.h"
#include "shell/common/gin_helper/pinnable.h"
#include "shell/common/gin_helper/wrappable.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace ui {
class SessionChangeObserver;
}

namespace lynxtron::api {

class PowerMonitor : public gin_helper::DeprecatedWrappable<PowerMonitor>,
                     public gin_helper::EventEmitterMixin<PowerMonitor>,
                     public gin_helper::Pinnable<PowerMonitor>,
                     public base::PowerStateObserver,
                     public base::PowerSuspendObserver {
 public:
  static gin_helper::Handle<PowerMonitor> Create(v8::Isolate* isolate);

  static gin::DeprecatedWrapperInfo kWrapperInfo;
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

  std::string GetSystemIdleState(gin_helper::ErrorThrower thrower,
                                 int idle_threshold);
  int GetSystemIdleTime() const;
  bool IsOnBatteryPower() const;
  bool GetOnBatteryPower() const;

  PowerMonitor(const PowerMonitor&) = delete;
  PowerMonitor& operator=(const PowerMonitor&) = delete;

 protected:
  explicit PowerMonitor(v8::Isolate* isolate);
  ~PowerMonitor() override;

 private:
  bool ShouldShutdown();

  void InitPlatformSpecificMonitors();
  void ShutdownPlatformSpecificMonitors();

  void OnBatteryPowerStatusChange(base::PowerStateObserver::BatteryPowerStatus
                                      battery_power_status) override;
  void OnSuspend() override;
  void OnResume() override;

#if BUILDFLAG(IS_WIN)
  void OnSessionChange(WPARAM wparam, const bool* is_current_session);

  std::unique_ptr<ui::SessionChangeObserver> session_change_observer_;
#endif
};

}  // namespace lynxtron::api

#endif  // LYNXTRON_SHELL_API_API_POWER_MONITOR_H_
