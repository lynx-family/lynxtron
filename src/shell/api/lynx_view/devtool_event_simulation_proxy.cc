// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/devtool_event_simulation_proxy.h"

#include <utility>

#include "lynx/platform/embedder/public/lynx_view.h"

namespace lynxtron::internal {

namespace {

constexpr char kRightMouseDownEvent[] = "rightmousedown";
constexpr char kRightMouseMoveEvent[] = "rightmousemove";
constexpr char kRightMouseUpEvent[] = "rightmouseup";

}  // namespace

LynxViewEventTarget::LynxViewEventTarget(lynx::pub::LynxView* view)
    : view_(view) {}

int LynxViewEventTarget::GetNodeForLocation(int x, int y) {
  return view_ ? view_->GetNodeForLocation(x, y) : 0;
}

void LynxViewEventTarget::SendTouchEvent(const std::string& name,
                                         int tag,
                                         int x,
                                         int y) {
  if (!view_) {
    return;
  }
  view_->SendTouchEvent(name, tag, x, y, x, y, x, y);
}

void LynxViewEventTarget::EmulateMouseEvent(const std::string& event_name,
                                            float x,
                                            float y,
                                            float delta_x,
                                            float delta_y) {
  if (!view_) {
    return;
  }
  view_->EmulateMouseEvent(event_name, x, y, delta_x, delta_y);
}

DevtoolEventSimulationProxy::DevtoolEventSimulationProxy(
    std::unique_ptr<DevtoolEventTarget> target)
    : target_(std::move(target)) {}

void DevtoolEventSimulationProxy::EmulateTouch(const std::string& event_type,
                                               int x,
                                               int y,
                                               const std::string& button,
                                               float delta_x,
                                               float delta_y,
                                               int modifiers,
                                               int click_count) {
  (void)modifiers;
  (void)click_count;
  if (!target_) {
    return;
  }
  if (event_type == kMouseWheel) {
    target_->EmulateMouseEvent("wheel", x, y, delta_x, delta_y);
    return;
  }
  if (button == kMouseRightButton || right_mouse_active_) {
    EmulateRightMouseEvent(event_type, x, y, delta_x, delta_y);
    return;
  }

  if (event_type == kMousePressed) {
    active_tag_ = target_->GetNodeForLocation(x, y);
    if (active_tag_ <= 0) {
      ResetTouchState();
      return;
    }
    touch_active_ = true;
    down_x_ = x;
    down_y_ = y;
    moved_beyond_tap_slop_ = false;
    target_->SendTouchEvent("touchstart", active_tag_, x, y);
    return;
  }

  if (!touch_active_ || active_tag_ <= 0) {
    return;
  }

  if (event_type == kMouseMoved) {
    UpdateTapState(x, y);
    target_->SendTouchEvent("touchmove", active_tag_, x, y);
    return;
  }

  if (event_type == kMouseReleased) {
    UpdateTapState(x, y);
    target_->SendTouchEvent("touchend", active_tag_, x, y);
    if (!moved_beyond_tap_slop_) {
      target_->SendTouchEvent("tap", active_tag_, x, y);
    }
    ResetTouchState();
  }
}

void DevtoolEventSimulationProxy::EmulateRightMouseEvent(
    const std::string& event_type,
    int x,
    int y,
    float delta_x,
    float delta_y) {
  if (event_type == kMousePressed) {
    right_mouse_active_ = true;
    target_->EmulateMouseEvent(kRightMouseDownEvent, x, y, delta_x, delta_y);
    return;
  }

  if (event_type == kMouseMoved) {
    target_->EmulateMouseEvent(
        right_mouse_active_ ? kRightMouseMoveEvent : "mousemove", x, y, delta_x,
        delta_y);
    return;
  }

  if (event_type == kMouseReleased) {
    target_->EmulateMouseEvent(kRightMouseUpEvent, x, y, delta_x, delta_y);
    right_mouse_active_ = false;
  }
}

void DevtoolEventSimulationProxy::UpdateTapState(int x, int y) {
  const int delta_x = x - down_x_;
  const int delta_y = y - down_y_;
  moved_beyond_tap_slop_ =
      moved_beyond_tap_slop_ ||
      (delta_x * delta_x + delta_y * delta_y) > (kTapSlopPx * kTapSlopPx);
}

void DevtoolEventSimulationProxy::ResetTouchState() {
  touch_active_ = false;
  moved_beyond_tap_slop_ = false;
  active_tag_ = 0;
  down_x_ = 0;
  down_y_ = 0;
}

}  // namespace lynxtron::internal
