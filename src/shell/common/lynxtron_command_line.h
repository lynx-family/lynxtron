// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_LYNXTRON_COMMAND_LINE_H_
#define LYNXTRON_SHELL_COMMON_LYNXTRON_COMMAND_LINE_H_

#include <string>
#include <vector>

#include "base/command_line.h"
#include "build/build_config.h"

namespace lynxtron {

// Singleton to remember the original "argc" and "argv".
class LynxtronCommandLine {
 public:
  // disable copy
  LynxtronCommandLine() = delete;
  LynxtronCommandLine(const LynxtronCommandLine&) = delete;
  LynxtronCommandLine& operator=(const LynxtronCommandLine&) = delete;

  static const base::CommandLine::StringVector& argv() { return argv_; }

  static std::vector<std::string> AsUtf8();

  static void Init(int argc, base::CommandLine::CharType const* const* argv);

 private:
  static base::CommandLine::StringVector argv_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_LYNXTRON_COMMAND_LINE_H_
