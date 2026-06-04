// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

const { Notification: LynxtronNotification } = process._linkedBinding(
  'lynxtron_binding_notification'
);

export default LynxtronNotification;
