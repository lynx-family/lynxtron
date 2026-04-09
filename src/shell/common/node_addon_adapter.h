// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_NODE_ADDON_ADAPTER_H_
#define LYNXTRON_SHELL_COMMON_NODE_ADDON_ADAPTER_H_

#include <windows.h>
#include <winternl.h>

#include "base/no_destructor.h"

enum {
  LDR_DLL_NOTIFICATION_REASON_LOADED = 1,
  LDR_DLL_NOTIFICATION_REASON_UNLOADED = 2,
};

typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
  ULONG Flags;                   // Reserved.
  PCUNICODE_STRING FullDllName;  // The full path name of the DLL module.
  PCUNICODE_STRING BaseDllName;  // The base file name of the DLL module.
  PVOID DllBase;      // A pointer to the base address for the DLL in memory.
  ULONG SizeOfImage;  // The size of the DLL image, in bytes.
} LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA {
  ULONG Flags;                   // Reserved.
  PCUNICODE_STRING FullDllName;  // The full path name of the DLL module.
  PCUNICODE_STRING BaseDllName;  // The base file name of the DLL module.
  PVOID DllBase;      // A pointer to the base address for the DLL in memory.
  ULONG SizeOfImage;  // The size of the DLL image, in bytes.
} LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

union LDR_DLL_NOTIFICATION_DATA {
  LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
  LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
};
using PLDR_DLL_NOTIFICATION_DATA = LDR_DLL_NOTIFICATION_DATA*;

namespace lynxtron {
class NodeAddonAdapter {
 public:
  static NodeAddonAdapter* Instance() {
    static base::NoDestructor<NodeAddonAdapter> instance;
    return instance.get();
  }

  NodeAddonAdapter(const NodeAddonAdapter&) = delete;
  NodeAddonAdapter& operator=(const NodeAddonAdapter&) = delete;

  bool Init();

  void UnInit();

  static void __stdcall DllNotificationCallback(
      unsigned long NotificationReason,
      const LDR_DLL_NOTIFICATION_DATA* NotificationData,
      void* Context);

 private:
  friend class base::NoDestructor<NodeAddonAdapter>;

  NodeAddonAdapter() = default;
  ~NodeAddonAdapter() = default;

  void* cookie_ = nullptr;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_NODE_ADDON_ADAPTER_H_
