// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE-CHROMIUM file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/win/scoped_hstring.h"

#include <winstring.h>

namespace lynxtron {

ScopedHString::ScopedHString(const wchar_t* source) {
  Reset(source);
}

ScopedHString::ScopedHString(const std::wstring& source) {
  Reset(source);
}

ScopedHString::ScopedHString() = default;

ScopedHString::~ScopedHString() {
  Reset();
}

void ScopedHString::Reset() {
  if (str_) {
    WindowsDeleteString(str_);
    str_ = nullptr;
  }
}

void ScopedHString::Reset(const wchar_t* source) {
  Reset();
  WindowsCreateString(source, wcslen(source), &str_);
}

void ScopedHString::Reset(const std::wstring& source) {
  Reset();
  WindowsCreateString(source.c_str(), source.length(), &str_);
}

}  // namespace lynxtron
