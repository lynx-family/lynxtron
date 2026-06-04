// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_power_monitor.h"

#include "base/functional/bind.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "shell/app/application.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/node_includes.h"
#if BUILDFLAG(IS_WIN)
#include "shell/ui/base/win/session_change_observer.h"
#endif
#include "ui/display/screen.h"

namespace lynxtron::api {

gin::DeprecatedWrapperInfo PowerMonitor::kWrapperInfo = {
    gin::kEmbedderNativeGin};

namespace {

constexpr char kInvalidIdleThresholdMessage[] =
    "Invalid idle threshold, must be greater than 0";

}  // namespace

PowerMonitor::PowerMonitor(v8::Isolate* isolate) {
#if BUILDFLAG(IS_MAC)
  if (Application::Get()) {
    Application::Get()->SetShutdownHandler(base::BindRepeating(
        &PowerMonitor::ShouldShutdown, base::Unretained(this)));
  }
#endif

  base::PowerMonitor::GetInstance()->AddPowerStateObserver(this);
  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);

  InitPlatformSpecificMonitors();
}

PowerMonitor::~PowerMonitor() {
  ShutdownPlatformSpecificMonitors();
  base::PowerMonitor::GetInstance()->RemovePowerStateObserver(this);
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
}

// static
gin_helper::Handle<PowerMonitor> PowerMonitor::Create(v8::Isolate* isolate) {
  auto handle = gin_helper::CreateHandle(isolate, new PowerMonitor(isolate));
  if (!handle.IsEmpty()) {
    handle->Pin(isolate);
  }
  return handle;
}

std::string PowerMonitor::GetSystemIdleState(gin_helper::ErrorThrower thrower,
                                             int idle_threshold) {
  if (idle_threshold <= 0) {
    thrower.ThrowTypeError(kInvalidIdleThresholdMessage);
    return "unknown";
  }

  display::Screen* screen = display::Screen::Get();
  if (!screen) {
    thrower.ThrowError("Failed to get screen information");
    return "unknown";
  }

  if (screen->IsScreenSaverActive()) {
    return "locked";
  }

  return screen->CalculateIdleTime() >= base::Seconds(idle_threshold)
             ? "idle"
             : "active";
}

int PowerMonitor::GetSystemIdleTime() const {
  display::Screen* screen = display::Screen::Get();
  if (!screen) {
    return 0;
  }

  return static_cast<int>(screen->CalculateIdleTime().InSeconds());
}

bool PowerMonitor::IsOnBatteryPower() const {
  auto* power_monitor = base::PowerMonitor::GetInstance();
  if (!power_monitor->IsInitialized()) {
    return false;
  }
  return power_monitor->IsOnBatteryPower();
}

bool PowerMonitor::GetOnBatteryPower() const {
  return IsOnBatteryPower();
}

bool PowerMonitor::ShouldShutdown() {
  return !Emit("shutdown");
}

void PowerMonitor::OnBatteryPowerStatusChange(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  switch (battery_power_status) {
    case base::PowerStateObserver::BatteryPowerStatus::kBatteryPower:
      Emit("on-battery");
      break;
    case base::PowerStateObserver::BatteryPowerStatus::kExternalPower:
      Emit("on-ac");
      break;
    case base::PowerStateObserver::BatteryPowerStatus::kUnknown:
      break;
  }
}

void PowerMonitor::OnSuspend() {
  Emit("suspend");
}

void PowerMonitor::OnResume() {
  Emit("resume");
}

gin::ObjectTemplateBuilder PowerMonitor::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin_helper::EventEmitterMixin<PowerMonitor>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("getSystemIdleState", &PowerMonitor::GetSystemIdleState)
      .SetMethod("getSystemIdleTime", &PowerMonitor::GetSystemIdleTime)
      .SetMethod("isOnBatteryPower", &PowerMonitor::IsOnBatteryPower)
      .SetProperty("onBatteryPower", &PowerMonitor::GetOnBatteryPower);
}

const char* PowerMonitor::GetTypeName() {
  return "PowerMonitor";
}

}  // namespace lynxtron::api

namespace {

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  gin_helper::Dictionary dict(isolate, exports);
  dict.Set("powerMonitor", lynxtron::api::PowerMonitor::Create(isolate));
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_binding_power_monitor, Initialize)
