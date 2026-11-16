// Copyright 2026 The Lynxtron Authors. All rights reserved.
#ifndef SHELL_APP_HARMONY_TOUCHPAD_INPUT_STATE_H_
#define SHELL_APP_HARMONY_TOUCHPAD_INPUT_STATE_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

namespace lynxtron {
struct HarmonyTouchpadEvent {
  int32_t window_id;
  int phase;  // Start, Update, End, Cancel.
  int32_t device;
  double x, y, pan_x, pan_y;
  size_t timestamp;
};

class HarmonyTouchpadInputState {
 public:
  std::vector<HarmonyTouchpadEvent> Update(int32_t window_id,
                                           int32_t device,
                                           int action,
                                           double x,
                                           double y,
                                           double delta_x,
                                           double delta_y,
                                           size_t timestamp);
  std::vector<HarmonyTouchpadEvent> CancelWindow(int32_t window_id,
                                                 size_t timestamp);

 private:
  struct Session {
    bool started = false;
    bool ended = false;
    double x = 0, y = 0, pan_x = 0, pan_y = 0;
  };
  std::mutex mutex_;
  std::map<std::pair<int32_t, int32_t>, Session> sessions_;
};
}  // namespace lynxtron
#endif  // SHELL_APP_HARMONY_TOUCHPAD_INPUT_STATE_H_
