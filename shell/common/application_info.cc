// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/common/application_info.h"

#include "base/no_destructor.h"
#include "base/strings/string_util.h"

namespace lynxtron {

std::string& OverriddenApplicationName() {
  static base::NoDestructor<std::string> overridden_application_name;
  return *overridden_application_name;
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
