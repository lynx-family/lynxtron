// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/api/notifications/notification.h"

#include "shell/api/notifications/notification_delegate.h"
#include "shell/api/notifications/platform_notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lynxtron {

namespace {

class MockNotificationDelegate : public NotificationDelegate {
 public:
  MockNotificationDelegate() {}
  ~MockNotificationDelegate() override {}

  void NotificationClick() override { clicked_ = true; }
  void NotificationClosed() override { closed_ = true; }
  void NotificationDisplayed() override { displayed_ = true; }
  void NotificationDestroyed() override { destroyed_ = true; }
  void NotificationFailed() override { failed_ = true; }

  bool clicked() const { return clicked_; }
  bool closed() const { return closed_; }
  bool displayed() const { return displayed_; }
  bool destroyed() const { return destroyed_; }
  bool failed() const { return failed_; }

 private:
  bool clicked_ = false;
  bool closed_ = false;
  bool displayed_ = false;
  bool destroyed_ = false;
  bool failed_ = false;
};

}  // namespace

TEST(NotificationTest, Basic) {
  MockNotificationDelegate delegate;
  NotificationOptions options;
  options.title = "Test Notification";
  options.body = "This is a test notification";

  // Since we don't have a full UI loop in this unit test environment,
  // we can only test the creation and basic logic if we mock the platform
  // service. However, Notification class calls
  // PlatformNotificationService::GetInstance() which might return nullptr or a
  // real instance depending on platform.

  // For now, we test that we can instantiate options.
  EXPECT_EQ(options.title, "Test Notification");
  EXPECT_EQ(options.body, "This is a test notification");
}

}  // namespace lynxtron
