// Copyright (c) 2025 Lynxtron, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_NOTIFICATIONS_WINDOWS_TOAST_ACTIVATOR_H_
#define LYNXTRON_SHELL_API_NOTIFICATIONS_WINDOWS_TOAST_ACTIVATOR_H_

#include <windows.h>

#include <NotificationActivationCallback.h>
#include <wrl/implements.h>

#include <string>
#include <vector>

namespace lynxtron {

class NotificationPlatformBridgeWin;

class NotificationActivator
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          INotificationActivationCallback> {
 public:
  NotificationActivator();
  ~NotificationActivator() override;

  IFACEMETHODIMP Activate(LPCWSTR app_user_model_id,
                          LPCWSTR invoked_args,
                          const NOTIFICATION_USER_INPUT_DATA* data,
                          ULONG data_count) override;

  static void RegisterActivator();
  static void UnregisterActivator();

 private:
  static DWORD g_cookie_;
  static bool g_registered_;
};

struct ActivationUserInput {
  std::wstring key;
  std::wstring value;
};

void HandleToastActivation(const std::wstring& invoked_args,
                           std::vector<ActivationUserInput> inputs);

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_NOTIFICATIONS_WINDOWS_TOAST_ACTIVATOR_H_
