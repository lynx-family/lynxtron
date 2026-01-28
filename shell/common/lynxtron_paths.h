// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_COMMON_LYNXTRON_PATHS_H_
#define LYNXTRON_SHELL_COMMON_LYNXTRON_PATHS_H_

#include "base/base_paths.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#elif BUILDFLAG(IS_MAC)
#include "base/base_paths_mac.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "base/base_paths_posix.h"
#endif

namespace lynxtron {

enum {
  PATH_START = 11000,

  DIR_USER_CACHE = PATH_START,  // Directory where user cache can be written.
  DIR_APP_LOGS,                 // Directory where app logs live.
  DIR_SESSION_DATA,             // Where cookies, localStorage are stored.

#if BUILDFLAG(IS_WIN)
  DIR_RECENT,  // Directory where recent files live
#endif

  DIR_CRASH_DUMPS,  // c.f. chrome::DIR_CRASH_DUMPS

  DIR_USER_DOCUMENTS,          // Directory for a user's "My Documents".
  DIR_USER_MUSIC,              // Directory for a user's music.
  DIR_USER_PICTURES,           // Directory for a user's pictures.
  DIR_USER_VIDEOS,             // Directory for a user's videos.
  DIR_DEFAULT_DOWNLOADS_SAFE,  // Directory for a user's
                               // "My Documents/Downloads", (Windows) or
                               // "Downloads". (Linux)
  DIR_DEFAULT_DOWNLOADS,       // Directory for a user's downloads.

  DIR_USER_DATA,
  DIR_APP_DICTIONARIES,

  // TODO(Guo Xi) : review PATH_END
  PATH_END,  // End of new paths. Those that follow redirect to base::DIR_*

#if BUILDFLAG(IS_WIN)
  DIR_APP_DATA = base::DIR_ROAMING_APP_DATA,
#elif BUILDFLAG(IS_MAC)
  DIR_APP_DATA = base::DIR_APP_DATA,
#endif

};

static_assert(PATH_START < PATH_END, "invalid PATH boundaries");

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_LYNXTRON_PATHS_H_
