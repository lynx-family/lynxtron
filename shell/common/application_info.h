// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_APPLICATION_INFO_H_
#define LYNXTRON_SHELL_COMMON_APPLICATION_INFO_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_hstring.h"
#endif

#include <string>

namespace lynxtron {

std::string& OverriddenApplicationName();
std::string& OverriddenApplicationVersion();

std::string GetPossiblyOverriddenApplicationName();

std::string GetApplicationName();
std::string GetApplicationVersion();
// Returns the user agent of lynxtron.
// std::string GetApplicationUserAgent();

// bool IsAppRTL();

#if BUILDFLAG(IS_WIN)
PCWSTR GetRawAppUserModelID();
bool GetAppUserModelID(base::win::ScopedHString* app_id);
void SetAppUserModelID(const std::wstring& name);
bool IsRunningInDesktopBridge();
#endif

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_APPLICATION_INFO_H_
