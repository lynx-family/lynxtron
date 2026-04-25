// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/win/reg_helper.h"

#include <Windows.h>

#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"

namespace lynxtron {

namespace {

constexpr wchar_t kSystemBiosInformationRegKey[] =
    L"HARDWARE\\DESCRIPTION\\System\\BIOS";
constexpr wchar_t kSystemManufacturerValueName[] = L"SystemManufacturer";

}  // namespace

bool RegHelper::GetDeviceManufacturer(std::string& manufacture) {
  base::win::RegKey bios_information_key;
  if (bios_information_key.Open(HKEY_LOCAL_MACHINE,
                                kSystemBiosInformationRegKey,
                                KEY_READ) != ERROR_SUCCESS) {
    return false;
  }

  std::wstring value16;
  if (bios_information_key.ReadValue(kSystemManufacturerValueName, &value16) !=
      ERROR_SUCCESS) {
    return false;
  }

  manufacture = base::WideToUTF8(value16);
  return true;
}

}  // namespace lynxtron
