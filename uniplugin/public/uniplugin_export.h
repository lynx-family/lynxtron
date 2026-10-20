// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef UNIPLUGIN_PUBLIC_UNIPLUGIN_EXPORT_H_
#define UNIPLUGIN_PUBLIC_UNIPLUGIN_EXPORT_H_

#if defined(_WIN32)
#if defined(BUILDING_UNIPLUGIN_SHARED)
#define UNIPLUGIN_EXPORT __declspec(dllexport)
#else
#define UNIPLUGIN_EXPORT __declspec(dllimport)
#endif  // defined(BUILDING_UNIPLUGIN_SHARED)

#else  // defined(_WIN32)
#if defined(BUILDING_UNIPLUGIN_SHARED)
#define UNIPLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define UNIPLUGIN_EXPORT
#endif  // defined(BUILDING_UNIPLUGIN_SHARED)
#endif

#endif  // UNIPLUGIN_PUBLIC_UNIPLUGIN_EXPORT_H_
