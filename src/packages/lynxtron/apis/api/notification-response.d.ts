// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

export interface NotificationResponse {
  /**
   * The identifier string of the action that the user selected.
   */
  actionIdentifier: string;
  /**
   * The delivery date of the notification.
   */
  date: number;
  /**
   * The unique identifier for this notification request.
   */
  identifier: string;
  /**
   * A dictionary of custom information associated with the notification.
   */
  userInfo: Record<string, any>;
  /**
   * The text entered or chosen by the user.
   */
  userText?: string;
}
