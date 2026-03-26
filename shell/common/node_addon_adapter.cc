// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/node_addon_adapter.h"

#include <iostream>

using PLDR_DLL_NOTIFICATION_FUNCTION =
    VOID(CALLBACK*)(ULONG notification_reason,
                    const LDR_DLL_NOTIFICATION_DATA* notification_data,
                    PVOID context);

using LdrRegisterDllNotificationFunc =
    NTSTATUS(NTAPI*)(ULONG flags,
                     PLDR_DLL_NOTIFICATION_FUNCTION notification_function,
                     PVOID context,
                     PVOID* cookie);

using LdrUnregisterDllNotificationFunc = NTSTATUS(NTAPI*)(PVOID cookie);

namespace lynxtron {

constexpr char kNodeExe[] = "node.exe";
constexpr char kMainDll[] = "lynxtron.dll";
constexpr wchar_t kNtDll[] = L"ntdll.dll";
constexpr char kLdrRegisterDllNotification[] = "LdrRegisterDllNotification";
constexpr char kLdrUnregisterDllNotification[] = "LdrUnregisterDllNotification";

bool NodeAddonAdapter::Init() {
  if (cookie_) {
    return true;
  }

  LdrRegisterDllNotificationFunc reg_fn =
      reinterpret_cast<LdrRegisterDllNotificationFunc>(::GetProcAddress(
          ::GetModuleHandle(kNtDll), kLdrRegisterDllNotification));
  if (!reg_fn) {
    return false;
  }

  NTSTATUS ret = reg_fn(0, &DllNotificationCallback, this, &cookie_);
  return NT_SUCCESS(ret);
}

void NodeAddonAdapter::UnInit() {
  if (!cookie_) {
    return;
  }

  LdrUnregisterDllNotificationFunc unreg_fn =
      reinterpret_cast<LdrUnregisterDllNotificationFunc>(::GetProcAddress(
          ::GetModuleHandle(kNtDll), kLdrUnregisterDllNotification));
  if (unreg_fn) {
    unreg_fn(cookie_);
    cookie_ = nullptr;
  }
}

void __stdcall NodeAddonAdapter::DllNotificationCallback(
    unsigned long NotificationReason,
    const LDR_DLL_NOTIFICATION_DATA* NotificationData,
    void* Context) {
  if (LDR_DLL_NOTIFICATION_REASON_LOADED != NotificationReason) {
    return;
  }
  if ((nullptr == NotificationData) ||
      (nullptr == NotificationData->Loaded.DllBase)) {
    return;
  }

  // check dos header
  PIMAGE_DOS_HEADER dos_header =
      (PIMAGE_DOS_HEADER)(NotificationData->Loaded.DllBase);
  if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
    return;
  }
  if (dos_header->e_lfanew < (LONG)sizeof(IMAGE_DOS_HEADER) ||
      dos_header->e_lfanew > MAXLONG) {
    return;
  }

  // check nt header
  PIMAGE_NT_HEADERS nt_headers =
      (PIMAGE_NT_HEADERS)((BYTE*)dos_header + dos_header->e_lfanew);
  if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
    return;
  }
  if (nt_headers->OptionalHeader.SizeOfImage <
      dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS)) {
    return;
  }

  // check delay import dir
  PIMAGE_DATA_DIRECTORY delay_import_directory =
      &nt_headers->OptionalHeader
           .DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
  if (delay_import_directory->VirtualAddress == 0 ||
      delay_import_directory->Size < sizeof(IMAGE_DELAYLOAD_DESCRIPTOR) ||
      delay_import_directory->VirtualAddress >=
          nt_headers->OptionalHeader.SizeOfImage ||
      delay_import_directory->VirtualAddress + delay_import_directory->Size >
          nt_headers->OptionalHeader.SizeOfImage) {
    return;
  }

  // enum delay load desc
  PIMAGE_DELAYLOAD_DESCRIPTOR delay_load_desc =
      (PIMAGE_DELAYLOAD_DESCRIPTOR)((BYTE*)NotificationData->Loaded.DllBase +
                                    delay_import_directory->VirtualAddress);
  SIZE_T max_descriptors =
      delay_import_directory->Size / sizeof(IMAGE_DELAYLOAD_DESCRIPTOR);
  SIZE_T current_descriptor = 0;
  while (delay_load_desc->DllNameRVA != 0 &&
         current_descriptor < max_descriptors) {
    if (delay_load_desc->DllNameRVA >= nt_headers->OptionalHeader.SizeOfImage) {
      break;
    }

    char* dll_name = (char*)((BYTE*)NotificationData->Loaded.DllBase +
                             delay_load_desc->DllNameRVA);
    if (lstrcmpiA(dll_name, kNodeExe) == 0) {
      DWORD old_protect = 0;
      SIZE_T protect_size = sizeof(kNodeExe);
      if (::VirtualProtect(dll_name, protect_size, PAGE_READWRITE,
                           &old_protect)) {
        strcpy_s(dll_name, protect_size, kMainDll);
        ::VirtualProtect(dll_name, protect_size, old_protect, &old_protect);
      }
      break;
    }
    delay_load_desc++;
    current_descriptor++;
  }
}
}  // namespace lynxtron
