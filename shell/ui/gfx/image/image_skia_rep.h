// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_GFX_IMAGE_IMAGE_SKIA_REP_H_
#define LYNXTRON_SHELL_UI_GFX_IMAGE_IMAGE_SKIA_REP_H_

#include "build/blink_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_IOS) && !BUILDFLAG(USE_BLINK)
#include "ui/gfx/image/image_skia_rep_ios.h"  // IWYU pragma: export
#else
#include "ui/gfx/image/image_skia_rep_default.h"  // IWYU pragma: export
#endif                                            // BUILDFLAG(IS_IOS)

#endif  // LYNXTRON_SHELL_UI_GFX_IMAGE_IMAGE_SKIA_REP_H_
