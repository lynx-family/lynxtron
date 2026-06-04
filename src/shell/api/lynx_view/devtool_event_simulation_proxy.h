// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_DEVTOOL_EVENT_SIMULATION_PROXY_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_DEVTOOL_EVENT_SIMULATION_PROXY_H_

#include <memory>
#include <string>

#include "lynx/platform/embedder/public/lynx_event_simulation_proxy.h"

namespace lynx {
namespace pub {
class LynxView;
}  // namespace pub
}  // namespace lynx

namespace lynxtron::internal {

class DevtoolEventTarget {
 public:
  virtual ~DevtoolEventTarget() = default;

  virtual int GetNodeForLocation(int x, int y) = 0;
  virtual void SendTouchEvent(const std::string& name,
                              int tag,
                              int x,
                              int y) = 0;
  virtual void EmulateMouseEvent(const std::string& event_name,
                                 float x,
                                 float y,
                                 float delta_x,
                                 float delta_y) = 0;
};

class LynxViewEventTarget : public DevtoolEventTarget {
 public:
  explicit LynxViewEventTarget(lynx::pub::LynxView* view);

  int GetNodeForLocation(int x, int y) override;
  void SendTouchEvent(const std::string& name, int tag, int x, int y) override;
  void EmulateMouseEvent(const std::string& event_name,
                         float x,
                         float y,
                         float delta_x,
                         float delta_y) override;

 private:
  lynx::pub::LynxView* view_;
};

class DevtoolEventSimulationProxy : public lynx::pub::LynxEventSimulationProxy {
 public:
  explicit DevtoolEventSimulationProxy(
      std::unique_ptr<DevtoolEventTarget> target);

  void EmulateTouch(const std::string& event_type,
                    int x,
                    int y,
                    const std::string& button,
                    float delta_x,
                    float delta_y,
                    int modifiers,
                    int click_count) override;

 private:
  static constexpr int kTapSlopPx = 5;

  void EmulateRightMouseEvent(const std::string& event_type,
                              int x,
                              int y,
                              float delta_x,
                              float delta_y);
  void UpdateTapState(int x, int y);
  void ResetTouchState();

  std::unique_ptr<DevtoolEventTarget> target_;
  bool right_mouse_active_ = false;
  bool touch_active_ = false;
  bool moved_beyond_tap_slop_ = false;
  int active_tag_ = 0;
  int down_x_ = 0;
  int down_y_ = 0;
};

}  // namespace lynxtron::internal

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_DEVTOOL_EVENT_SIMULATION_PROXY_H_
