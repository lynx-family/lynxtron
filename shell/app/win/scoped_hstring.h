// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE-CHROMIUM file.

#ifndef LYNXTRON_SHELL_APP_WIN_SCOPED_HSTRING_H_
#define LYNXTRON_SHELL_APP_WIN_SCOPED_HSTRING_H_

#include <windows.h>

#include <hstring.h>

#include <string>

namespace lynxtron {

class ScopedHString {
 public:
  // Copy from |source|.
  explicit ScopedHString(const wchar_t* source);
  explicit ScopedHString(const std::wstring& source);
  // Create empty string.
  ScopedHString();
  ~ScopedHString();

  // disable copy
  ScopedHString(const ScopedHString&) = delete;
  ScopedHString& operator=(const ScopedHString&) = delete;

  // Sets to |source|.
  void Reset();
  void Reset(const wchar_t* source);
  void Reset(const std::wstring& source);

  // Returns string.
  operator HSTRING() const { return str_; }

  // Whether there is a string created.
  bool success() const { return str_; }

 private:
  HSTRING str_ = nullptr;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_WIN_SCOPED_HSTRING_H_
