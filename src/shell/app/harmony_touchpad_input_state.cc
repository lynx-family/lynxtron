// Copyright 2026 The Lynxtron Authors. All rights reserved.
#include "shell/app/harmony_touchpad_input_state.h"

namespace lynxtron {
std::vector<HarmonyTouchpadEvent> HarmonyTouchpadInputState::Update(
    int32_t window_id,
    int32_t device,
    int action,
    double x,
    double y,
    double delta_x,
    double delta_y,
    size_t timestamp) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& session = sessions_[{window_id, device}];
  std::vector<HarmonyTouchpadEvent> events;
  auto emit = [&](int phase) {
    events.push_back({window_id, phase, 0x30000000 | (device & 0x0fffffff),
                      session.x, session.y, session.pan_x, session.pan_y,
                      timestamp});
  };
  if (action == 1) {
    if (session.started) {
      emit(3);
    }
    session = {};
  }
  if (action == 3 || action == 4) {
    if (session.started) {
      emit(action == 3 ? 2 : 3);
    }
    session.started = false;
    session.ended = true;
    return events;
  }
  if ((action != 1 && action != 2) || session.ended ||
      (delta_x == 0 && delta_y == 0)) {
    return events;
  }
  session.x = x;
  session.y = y;
  if (!session.started) {
    session.started = true;
    emit(0);
  }
  session.pan_x -= delta_x;
  session.pan_y -= delta_y;
  emit(1);
  return events;
}

std::vector<HarmonyTouchpadEvent> HarmonyTouchpadInputState::CancelWindow(
    int32_t window_id,
    size_t timestamp) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<HarmonyTouchpadEvent> events;
  for (auto& [key, session] : sessions_) {
    if (key.first != window_id) {
      continue;
    }
    if (session.started) {
      events.push_back({window_id, 3, 0x30000000 | (key.second & 0x0fffffff),
                        session.x, session.y, session.pan_x, session.pan_y,
                        timestamp});
    }
    session.started = false;
    session.ended = true;
  }
  return events;
}
}  // namespace lynxtron
