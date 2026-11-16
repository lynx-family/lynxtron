// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Standalone regression test; compile with the state implementation and -Isrc.
#include "shell/app/harmony_touchpad_input_state.h"

#include <cassert>
int main() {
  lynxtron::HarmonyTouchpadInputState s;
  auto update = [&](int window, int device, int action, double dx, double dy) {
    return s.Update(window, device, action, 20, 30, dx, dy, 100);
  };
  assert(update(1, 7, 1, 0, 0).empty());
  auto e = update(1, 7, 2, 2, -3);
  assert(e.size() == 2 && e[0].phase == 0 && e[1].phase == 1);
  assert(e[1].pan_x == -2 && e[1].pan_y == 3);
  assert(e[0].device == 0x30000007);
  e = update(1, 7, 2, -1, 1);
  assert(e.size() == 1 && e[0].pan_x == -1 && e[0].pan_y == 2);
  assert(update(1, 7, 3, 0, 0)[0].phase == 2);
  assert(update(1, 7, 2, 1, 1).empty());
  assert(update(1, 7, 1, 1, 1).size() == 2);
  e = update(1, 7, 1, 1, 1);
  assert(e.size() == 3 && e[0].phase == 3);
  assert(update(2, 7, 2, 1, 1).size() == 2);
  assert(update(1, 8, 2, 1, 1).size() == 2);
  assert(s.CancelWindow(1, 101).size() == 2);
  assert(s.CancelWindow(1, 102).empty());
  assert(update(1, 7, 2, 1, 1).empty());
  assert(update(2, 7, 4, 0, 0)[0].phase == 3);
  assert(update(2, 7, 2, 1, 1).empty());
}
