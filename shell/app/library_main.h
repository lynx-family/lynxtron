// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_APP_LYNXTRON_LIBRARY_MAIN_H_
#define LYNXTRON_SHELL_APP_LYNXTRON_LIBRARY_MAIN_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#define LYNXTRON_EXPORT __declspec(dllexport)
using LynxtronMainPtr = int (*)();
extern "C" {
LYNXTRON_EXPORT int LynxtronMain();
}
#else
#define LYNXTRON_EXPORT __attribute__((visibility("default")))
extern "C" {
using LynxtronMainPtr = int (*)(int, char*[]);
LYNXTRON_EXPORT int LynxtronMain(int argc, char* argv[]);
}
#endif

#endif  // LYNXTRON_SHELL_APP_LIBRARY_MAIN_H_
