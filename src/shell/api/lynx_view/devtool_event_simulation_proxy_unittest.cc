// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/devtool_event_simulation_proxy.h"

#include <string>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace lynxtron::internal {
namespace {

struct TouchEventRecord {
  std::string name;
  int tag;
  int x;
  int y;
};

struct MouseEventRecord {
  std::string name;
  float x;
  float y;
  float delta_x;
  float delta_y;
};

class FakeDevtoolEventTarget : public DevtoolEventTarget {
 public:
  explicit FakeDevtoolEventTarget(int node_for_location)
      : node_for_location_(node_for_location) {}

  int GetNodeForLocation(int x, int y) override {
    hit_test_requests.emplace_back(x, y);
    return node_for_location_;
  }

  void SendTouchEvent(const std::string& name, int tag, int x, int y) override {
    touch_events.push_back({name, tag, x, y});
  }

  void EmulateMouseEvent(const std::string& event_name,
                         float x,
                         float y,
                         float delta_x,
                         float delta_y) override {
    mouse_events.push_back({event_name, x, y, delta_x, delta_y});
  }

  std::vector<std::pair<int, int>> hit_test_requests;
  std::vector<TouchEventRecord> touch_events;
  std::vector<MouseEventRecord> mouse_events;

 private:
  int node_for_location_;
};

class DevtoolEventSimulationProxyTest : public testing::Test {
 protected:
  DevtoolEventSimulationProxy CreateProxy(FakeDevtoolEventTarget** out_target,
                                          int node_for_location = 42) {
    auto target = std::make_unique<FakeDevtoolEventTarget>(node_for_location);
    *out_target = target.get();
    return DevtoolEventSimulationProxy(std::move(target));
  }
};

TEST_F(DevtoolEventSimulationProxyTest, PressMoveReleaseBeyondTapSlopSkipsTap) {
  FakeDevtoolEventTarget* target = nullptr;
  auto proxy = CreateProxy(&target);

  proxy.EmulateTouch(lynx::pub::LynxEventSimulationProxy::kMousePressed, 100,
                     200, lynx::pub::LynxEventSimulationProxy::kMouseLeftButton,
                     0, 0, 0, 1);
  proxy.EmulateTouch(lynx::pub::LynxEventSimulationProxy::kMouseMoved, 110, 200,
                     lynx::pub::LynxEventSimulationProxy::kMouseLeftButton, 0,
                     0, 0, 1);
  proxy.EmulateTouch(lynx::pub::LynxEventSimulationProxy::kMouseReleased, 110,
                     200, lynx::pub::LynxEventSimulationProxy::kMouseLeftButton,
                     0, 0, 0, 1);

  ASSERT_EQ(target->hit_test_requests.size(), 1u);
  EXPECT_EQ(target->hit_test_requests[0], std::make_pair(100, 200));
  ASSERT_EQ(target->touch_events.size(), 3u);
  EXPECT_EQ(target->touch_events[0].name, "touchstart");
  EXPECT_EQ(target->touch_events[1].name, "touchmove");
  EXPECT_EQ(target->touch_events[2].name, "touchend");
  EXPECT_EQ(target->touch_events[0].tag, 42);
  EXPECT_EQ(target->touch_events[2].x, 110);
  EXPECT_EQ(target->touch_events[2].y, 200);
}

TEST_F(DevtoolEventSimulationProxyTest, ReleaseWithinTapSlopSendsTap) {
  FakeDevtoolEventTarget* target = nullptr;
  auto proxy = CreateProxy(&target);

  proxy.EmulateTouch(lynx::pub::LynxEventSimulationProxy::kMousePressed, 10, 20,
                     lynx::pub::LynxEventSimulationProxy::kMouseLeftButton, 0,
                     0, 0, 1);
  proxy.EmulateTouch(lynx::pub::LynxEventSimulationProxy::kMouseReleased, 13,
                     24, lynx::pub::LynxEventSimulationProxy::kMouseLeftButton,
                     0, 0, 0, 1);

  ASSERT_EQ(target->touch_events.size(), 3u);
  EXPECT_EQ(target->touch_events[0].name, "touchstart");
  EXPECT_EQ(target->touch_events[1].name, "touchend");
  EXPECT_EQ(target->touch_events[2].name, "tap");
  EXPECT_EQ(target->touch_events[2].x, 13);
  EXPECT_EQ(target->touch_events[2].y, 24);
}

TEST_F(DevtoolEventSimulationProxyTest, InvalidHitTestDoesNotSendEvents) {
  FakeDevtoolEventTarget* target = nullptr;
  auto proxy = CreateProxy(&target, 0);

  proxy.EmulateTouch(lynx::pub::LynxEventSimulationProxy::kMousePressed, 1, 2,
                     lynx::pub::LynxEventSimulationProxy::kMouseLeftButton, 0,
                     0, 0, 1);
  proxy.EmulateTouch(lynx::pub::LynxEventSimulationProxy::kMouseReleased, 1, 2,
                     lynx::pub::LynxEventSimulationProxy::kMouseLeftButton, 0,
                     0, 0, 1);

  ASSERT_EQ(target->hit_test_requests.size(), 1u);
  EXPECT_TRUE(target->touch_events.empty());
  EXPECT_TRUE(target->mouse_events.empty());
}

TEST_F(DevtoolEventSimulationProxyTest, RightClickUsesMousePath) {
  FakeDevtoolEventTarget* target = nullptr;
  auto proxy = CreateProxy(&target);

  proxy.EmulateTouch(lynx::pub::LynxEventSimulationProxy::kMousePressed, 5, 6,
                     lynx::pub::LynxEventSimulationProxy::kMouseRightButton, 0,
                     0, 0, 1);
  proxy.EmulateTouch(lynx::pub::LynxEventSimulationProxy::kMouseMoved, 7, 8,
                     lynx::pub::LynxEventSimulationProxy::kMouseRightButton, 0,
                     0, 0, 1);
  proxy.EmulateTouch(lynx::pub::LynxEventSimulationProxy::kMouseReleased, 7, 8,
                     lynx::pub::LynxEventSimulationProxy::kMouseRightButton, 0,
                     0, 0, 1);

  EXPECT_TRUE(target->hit_test_requests.empty());
  EXPECT_TRUE(target->touch_events.empty());
  ASSERT_EQ(target->mouse_events.size(), 3u);
  EXPECT_EQ(target->mouse_events[0].name, "rightmousedown");
  EXPECT_EQ(target->mouse_events[1].name, "rightmousemove");
  EXPECT_EQ(target->mouse_events[2].name, "rightmouseup");
  EXPECT_FLOAT_EQ(target->mouse_events[1].x, 7.f);
  EXPECT_FLOAT_EQ(target->mouse_events[1].y, 8.f);
}

TEST_F(DevtoolEventSimulationProxyTest, WheelUsesMousePath) {
  FakeDevtoolEventTarget* target = nullptr;
  auto proxy = CreateProxy(&target);

  proxy.EmulateTouch(lynx::pub::LynxEventSimulationProxy::kMouseWheel, 7, 8,
                     lynx::pub::LynxEventSimulationProxy::kMouseLeftButton,
                     1.5f, -2.5f, 0, 0);

  EXPECT_TRUE(target->hit_test_requests.empty());
  EXPECT_TRUE(target->touch_events.empty());
  ASSERT_EQ(target->mouse_events.size(), 1u);
  EXPECT_EQ(target->mouse_events[0].name, "wheel");
  EXPECT_FLOAT_EQ(target->mouse_events[0].delta_x, 1.5f);
  EXPECT_FLOAT_EQ(target->mouse_events[0].delta_y, -2.5f);
}

}  // namespace
}  // namespace lynxtron::internal
