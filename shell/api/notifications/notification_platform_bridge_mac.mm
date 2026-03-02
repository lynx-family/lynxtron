// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/api/notifications/notification_platform_bridge_mac.h"

#include <map>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "shell/api/notifications/notification_center_delegate.h"

namespace lynxtron {

struct NotificationPlatformBridgeMac::ObjCStorage {
  NotificationCenterDelegate* __strong notification_center_delegate;
  std::set<MacNotification*> notifications;
};

class MacNotification : public Notification {
 public:
  MacNotification(NotificationDelegate* delegate,
                  NotificationOptions* options,
                  NotificationPlatformBridgeMac* presenter);

  ~MacNotification() override;

  void Show() override {
    notification_ = [[NSUserNotification alloc] init];

    NSString* identifier =
        [NSString stringWithFormat:@"%@:notification:%@",
                                   [[NSBundle mainBundle] bundleIdentifier],
                                   [[NSUUID UUID] UUIDString]];

    [notification_ setTitle:base::SysUTF8ToNSString(options_.title)];
    [notification_ setSubtitle:base::SysUTF8ToNSString(options_.subtitle)];
    [notification_ setInformativeText:base::SysUTF8ToNSString(options_.body)];
    [notification_ setIdentifier:identifier];

    if (options_.silent) {
      [notification_ setSoundName:nil];
    } else if (options_.sound.empty()) {
      [notification_ setSoundName:NSUserNotificationDefaultSoundName];
    } else {
      [notification_ setSoundName:base::SysUTF8ToNSString(options_.sound)];
    }

    if (options_.has_reply) {
      [notification_ setHasReplyButton:true];
      [notification_ setResponsePlaceholder:base::SysUTF8ToNSString(
                                                options_.reply_placeholder)];
    }

    // We need to explicitly set this to false if there are no
    // actions, otherwise a Show button will appear by default.
    if (options_.actions.size() == 0) {
      [notification_ setHasActionButton:false];
    }

    int i = 0;
    action_index_ = UINT_MAX;
    NSMutableArray* additionalActions = [[NSMutableArray alloc] init];
    for (const auto& action : options_.actions) {
      // If the notification has both a reply and actions,
      // the reply takes precedence and the actions all
      // become additional actions.
      if (!options_.has_reply && action_index_ == UINT_MAX) {
        // First button observed is the displayed action
        [notification_
            setActionButtonTitle:base::SysUTF8ToNSString(action.text)];
        action_index_ = i;
      } else {
        // All of the rest are appended to the list of additional actions
        NSString* actionIdentifier =
            [NSString stringWithFormat:@"%@Action%d", identifier, i];
        NSUserNotificationAction* notificationAction = [NSUserNotificationAction
            actionWithIdentifier:actionIdentifier
                           title:base::SysUTF8ToNSString(action.text)];
        [additionalActions addObject:notificationAction];
        additional_action_indices_.emplace(
            base::SysNSStringToUTF8(actionIdentifier), i);
      }
      i++;
    }

    if ([additionalActions count] > 0) {
      [notification_ setAdditionalActions:additionalActions];
    }

    if (!options_.close_button_text.empty()) {
      [notification_ setOtherButtonTitle:base::SysUTF8ToNSString(
                                             options_.close_button_text)];
    }

    [NSUserNotificationCenter.defaultUserNotificationCenter
        deliverNotification:notification_];
  }

  void Dismiss() override {
    if (notification_) {
      [NSUserNotificationCenter.defaultUserNotificationCenter
          removeDeliveredNotification:notification_];
    }

    NotificationDismissed();

    notification_ = nil;
  }

  void NotificationDisplayed() {
    if (delegate_) {
      delegate_->NotificationDisplayed();
    }
  }

  void NotificationClicked() {
    if (delegate_) {
      delegate_->NotificationClick();
    }
  }

  void NotificationReplied(const std::string& reply) {
    if (delegate_) {
      delegate_->NotificationReplied(reply);
    }
  }

  void NotificationActivated() {
    if (delegate_) {
      delegate_->NotificationAction(action_index_);
    }
  }

  void NotificationActivated(NSUserNotificationAction* action) {
    if (delegate_) {
      unsigned index = action_index_;
      std::string identifier = base::SysNSStringToUTF8(action.identifier);
      auto it = additional_action_indices_.find(identifier);
      if (it != additional_action_indices_.end()) {
        index = it->second;
      }
      delegate_->NotificationAction(index);
    }
  }

  void NotificationDismissed() {
    if (delegate_) {
      delegate_->NotificationClosed();
    }
  }

  NSUserNotification* notification() const { return notification_; }

 private:
  NSUserNotification* __strong notification_;
  NotificationPlatformBridgeMac* presenter_;
  unsigned action_index_;
  std::map<std::string, unsigned> additional_action_indices_;
};

MacNotification::MacNotification(NotificationDelegate* delegate,
                                 NotificationOptions* options,
                                 NotificationPlatformBridgeMac* presenter)
    : Notification(delegate, options), presenter_(presenter) {
  presenter_->objc_storage_->notifications.insert(this);
}

MacNotification::~MacNotification() {
  if (notification_) {
    [NSUserNotificationCenter.defaultUserNotificationCenter
        removeDeliveredNotification:notification_];
  }
  presenter_->objc_storage_->notifications.erase(this);
}

NotificationPlatformBridgeMac::NotificationPlatformBridgeMac()
    : objc_storage_(std::make_unique<ObjCStorage>()) {
  objc_storage_->notification_center_delegate =
      [[NotificationCenterDelegate alloc] initWithPresenter:this];
  NSUserNotificationCenter.defaultUserNotificationCenter.delegate =
      objc_storage_->notification_center_delegate;
}

NotificationPlatformBridgeMac::~NotificationPlatformBridgeMac() {
  NSUserNotificationCenter.defaultUserNotificationCenter.delegate = nil;
}

std::unique_ptr<Notification> NotificationPlatformBridgeMac::CreateNotification(
    NotificationDelegate* delegate,
    NotificationOptions* options) {
  return std::make_unique<MacNotification>(delegate, options, this);
}

MacNotification* NotificationPlatformBridgeMac::GetNotification(
    NSUserNotification* ns_notification) {
  for (MacNotification* notification : objc_storage_->notifications) {
    if ([notification->notification().identifier
            isEqual:ns_notification.identifier]) {
      return notification;
    }
  }
  return nullptr;
}

}  // namespace lynxtron

@implementation NotificationCenterDelegate

- (instancetype)initWithPresenter:
    (lynxtron::NotificationPlatformBridgeMac*)presenter {
  self = [super init];
  if (!self) {
    return nil;
  }

  presenter_ = presenter;
  return self;
}

- (void)userNotificationCenter:(NSUserNotificationCenter*)center
        didDeliverNotification:(NSUserNotification*)notif {
  auto* notification = presenter_->GetNotification(notif);
  if (notification) {
    notification->NotificationDisplayed();
  }
}

- (void)userNotificationCenter:(NSUserNotificationCenter*)center
       didActivateNotification:(NSUserNotification*)notif {
  auto* notification = presenter_->GetNotification(notif);

  if (notification) {
    // Ref:
    // https://developer.apple.com/documentation/foundation/nsusernotificationactivationtype?language=objc
    if (notif.activationType ==
        NSUserNotificationActivationTypeContentsClicked) {
      notification->NotificationClicked();
    } else if (notif.activationType ==
               NSUserNotificationActivationTypeActionButtonClicked) {
      notification->NotificationActivated();
    } else if (notif.activationType ==
               NSUserNotificationActivationTypeReplied) {
      notification->NotificationReplied([notif.response.string UTF8String]);
    } else {
      if (notif.activationType ==
          NSUserNotificationActivationTypeAdditionalActionClicked) {
        notification->NotificationActivated([notif additionalActivationAction]);
      }
    }
  }
}

- (BOOL)userNotificationCenter:(NSUserNotificationCenter*)center
     shouldPresentNotification:(NSUserNotification*)notification {
  // Display notifications even if the app is active.
  return YES;
}

// This undocumented method notifies us if a user closes "Alert" notifications
// https://chromium.googlesource.com/chromium/src/+/lkgr/chrome/browser/notifications/notification_platform_bridge_mac.mm
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
               didDismissAlert:(NSUserNotification*)notif {
  auto* notification = presenter_->GetNotification(notif);
  if (notification) {
    notification->NotificationDismissed();
  }
}

// This undocumented method notifies us if a user closes "Banner" notifications
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
    didRemoveDeliveredNotifications:(NSArray*)notifications {
  for (NSUserNotification* notif in notifications) {
    auto* notification = presenter_->GetNotification(notif);
    if (notification) {
      notification->NotificationDismissed();
    }
  }
}

@end
