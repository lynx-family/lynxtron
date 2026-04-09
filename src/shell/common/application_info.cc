// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/application_info.h"

#include "base/no_destructor.h"
#include "base/strings/string_util.h"

namespace lynxtron {

std::string& OverriddenApplicationName() {
  static base::NoDestructor<std::string> overridden_application_name;
  return *overridden_application_name;
}

std::string& OverriddenApplicationId() {
  static base::NoDestructor<std::string> id;
  return *id;
}

std::string& OverriddenApplicationVersion() {
  static base::NoDestructor<std::string> overridden_application_version;
  return *overridden_application_version;
}

std::string GetPossiblyOverriddenApplicationName() {
  std::string ret = OverriddenApplicationName();
  if (!ret.empty()) {
    return ret;
  }
  return GetApplicationName();
}
}  // namespace lynxtron
