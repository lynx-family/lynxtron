// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/common/path_provider.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "shell/common/application_info.h"
#include "shell/common/lynxtron_paths.h"
#include "shell/common/platform_util.h"
#include "shell/common/thread_restrictions.h"

#if BUILDFLAG(IS_WIN)
#include <shobjidl.h>
#include <windows.h>

#include <knownfolders.h>
#include <shellapi.h>
#include <shlobj.h>

#include "base/base_paths_win.h"
#include "base/win/scoped_co_mem.h"
#elif BUILDFLAG(IS_MAC)
#include "base/base_paths_mac.h"
#endif

namespace lynxtron {
#if BUILDFLAG(IS_WIN)
namespace {

// Generic function to call SHGetFolderPath().
bool GetUserDirectory(int csidl_folder, base::FilePath* result) {
  // We need to go compute the value. It would be nice to support paths
  // with names longer than MAX_PATH, but the system functions don't seem
  // to be designed for it either, with the exception of GetTempPath
  // (but other things will surely break if the temp path is too long,
  // so we don't bother handling it.
  wchar_t path_buf[MAX_PATH];
  path_buf[0] = 0;
  if (FAILED(::SHGetFolderPath(nullptr, csidl_folder, nullptr,
                               SHGFP_TYPE_CURRENT, path_buf))) {
    return false;
  }
  *result = base::FilePath(path_buf);
  return true;
}

}  // namespace

void GetUserCacheDirectory(const base::FilePath& profile_dir,
                           base::FilePath* result) {
  // This function does more complicated things on Mac/Linux.
  *result = profile_dir;
}

bool GetUserDocumentsDirectory(base::FilePath* result) {
  return GetUserDirectory(CSIDL_MYDOCUMENTS, result);
}

// Return a default path for downloads that is safe.
// We just use 'Downloads' under DIR_USER_DOCUMENTS. Localizing
// 'downloads' is not a good idea because Chrome's UI language
// can be changed.
bool GetUserDownloadsDirectorySafe(base::FilePath* result) {
  if (!GetUserDocumentsDirectory(result)) {
    return false;
  }

  *result = result->Append(L"Downloads");
  return true;
}

// Get the downloads known folder. Since it can be relocated to point to a
// "dangerous" folder, callers should validate that the returned path is not
// dangerous before using it.
bool GetUserDownloadsDirectory(base::FilePath* result) {
  base::win::ScopedCoMem<wchar_t> path_buf;
  if (SUCCEEDED(
          ::SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path_buf))) {
    *result = base::FilePath(std::wstring(path_buf));
    return true;
  }
  return GetUserDownloadsDirectorySafe(result);
}

bool GetUserMusicDirectory(base::FilePath* result) {
  return GetUserDirectory(CSIDL_MYMUSIC, result);
}

bool GetUserPicturesDirectory(base::FilePath* result) {
  return GetUserDirectory(CSIDL_MYPICTURES, result);
}

bool GetUserVideosDirectory(base::FilePath* result) {
  return GetUserDirectory(CSIDL_MYVIDEO, result);
}
#endif

bool PathProvider(int key, base::FilePath* result) {
  bool create_dir = false;
  base::FilePath cur;
  switch (key) {
    case DIR_USER_DATA:
      if (!base::PathService::Get(DIR_APP_DATA, &cur)) {
        return false;
      }
      cur = cur.Append(base::FilePath::FromUTF8Unsafe(
          lynxtron::GetPossiblyOverriddenApplicationName()));
      create_dir = true;
      break;
    // case DIR_CRASH_DUMPS:
    //   if (!base::PathService::Get(DIR_USER_DATA, &cur))
    //     return false;
    //   cur = cur.Append(FILE_PATH_LITERAL("Crashpad"));
    //   create_dir = true;
    //   break;
    case DIR_APP_DICTIONARIES:
      // TODO(nornagon): can we just default to using Chrome's logic here?
      if (!base::PathService::Get(DIR_USER_DATA, &cur)) {
        return false;
      }
      cur = cur.Append(base::FilePath::FromUTF8Unsafe("Dictionaries"));
      create_dir = true;
      break;
    case DIR_USER_CACHE: {
#if defined(OS_POSIX)
      int parent_key = base::DIR_CACHE;
#else
      // On Windows, there's no OS-level centralized location for caches, so
      // store the cache in the app data directory.
      // TODO(Guo Xi):DIR_USER_STARTUP
      int parent_key = base::DIR_USER_STARTUP;
#endif
      if (!base::PathService::Get(parent_key, &cur)) {
        return false;
      }
      cur = cur.Append(base::FilePath::FromUTF8Unsafe(
          lynxtron::GetPossiblyOverriddenApplicationName()));
      create_dir = true;
      break;
    }
#if defined(OS_LINUX)
    case DIR_APP_DATA: {
      auto env = base::Environment::Create();
      cur = base::nix::GetXDGDirectory(
          env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir);
      break;
    }
#endif
#if BUILDFLAG(IS_WIN)
    case DIR_RECENT:
      if (!platform_util::GetFolderPath(DIR_RECENT, &cur)) {
        return false;
      }
      create_dir = true;
      break;
#endif
    case DIR_APP_LOGS:
#if BUILDFLAG(IS_MAC)
      if (!base::PathService::Get(base::DIR_HOME, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("Library"));
      cur = cur.Append(FILE_PATH_LITERAL("Logs"));
      cur = cur.Append(base::FilePath::FromUTF8Unsafe(
          lynxtron::GetPossiblyOverriddenApplicationName()));
#else
      if (!base::PathService::Get(DIR_USER_DATA, &cur)) {
        return false;
      }
      cur = cur.Append(base::FilePath::FromUTF8Unsafe("logs"));
#endif
      create_dir = true;
      break;
#if BUILDFLAG(IS_WIN)
    case DIR_USER_DOCUMENTS:
      if (!GetUserDocumentsDirectory(&cur)) {
        return false;
      }
      create_dir = true;
      break;
    case DIR_USER_MUSIC:
      if (!GetUserMusicDirectory(&cur)) {
        return false;
      }
      break;
    case DIR_USER_PICTURES:
      if (!GetUserPicturesDirectory(&cur)) {
        return false;
      }
      break;
    case DIR_USER_VIDEOS:
      if (!GetUserVideosDirectory(&cur)) {
        return false;
      }
      break;
#endif
    case DIR_DEFAULT_DOWNLOADS_SAFE:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      if (!GetUserDownloadsDirectorySafe(&cur)) {
        return false;
      }
      break;
#else
      // Fall through for all other platforms.
#endif
    case DIR_DEFAULT_DOWNLOADS:
#if BUILDFLAG(IS_ANDROID)
      if (!base::android::GetDownloadsDirectory(&cur)) {
        return false;
      }
#elif BUILDFLAG(IS_WIN)
      if (!GetUserDownloadsDirectory(&cur)) {
        return false;
      }
      // Do not create the download directory here, we have done it twice now
      // and annoyed a lot of users.
#endif
      break;
    default:
      return false;
  }

  // TODO(bauerb): http://crbug.com/259796
  ScopedAllowBlockingForLynxtron allow_blocking;
  if (create_dir && !base::PathExists(cur) && !base::CreateDirectory(cur)) {
    return false;
  }

  *result = cur;

  return true;
}
}  // namespace lynxtron
