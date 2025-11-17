// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/common/lynxtron_command_line.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/strings/utf_string_conversions.h"

namespace lynxtron {

// static
base::CommandLine::StringVector LynxtronCommandLine::argv_;

// static
void LynxtronCommandLine::Init(int argc,
                               base::CommandLine::CharType const* const* argv) {
  DCHECK(argv_.empty());

  // Safety: as is normal in command lines, argc and argv must correspond
  // to one another. Otherwise there will be out-of-bounds accesses.
  argv_.assign(argv, UNSAFE_BUFFERS(argv + argc));
}

// static
std::vector<std::string> LynxtronCommandLine::AsUtf8() {
  DCHECK(!argv_.empty());

#if BUILDFLAG(IS_WIN)
  return base::ToVector(
      argv_, [](const auto& wstr) { return base::WideToUTF8(wstr); });
#else
  return argv_;
#endif
}

#if BUILDFLAG(IS_LINUX)
// static
void LynxtronCommandLine::InitializeFromCommandLine() {
  argv_ = base::CommandLine::ForCurrentProcess()->argv();
}
#endif

}  // namespace lynxtron
