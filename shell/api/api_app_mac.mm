// Copyright (c) 2019 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_app.h"

#import <Cocoa/Cocoa.h>
#import <sys/sysctl.h>

#include <string>

#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "shell/app/icon_manager.h"
#include "shell/app/javascript_environment.h"
#include "shell/app/main_parts.h"
#include "shell/common/gin_converters/image_converter.h"
#include "shell/common/lynxtron_paths.h"
#include "shell/common/node_includes.h"
#include "shell/common/thread_restrictions.h"
#include "ui/gfx/image/image.h"

namespace lynxtron::api {

void OnIconDataAvailable(gin_helper::Promise<gfx::Image> promise,
                         gfx::Image icon) {
  if (!icon.IsEmpty()) {
    promise.Resolve(icon);
  } else {
    promise.RejectWithErrorMessage("Failed to get file icon.");
  }
}

void App::SetAppLogsPath(gin_helper::ErrorThrower thrower,
                         std::optional<base::FilePath> custom_path) {
  if (custom_path.has_value()) {
    if (!custom_path->IsAbsolute()) {
      thrower.ThrowError("Path must be absolute");
      return;
    }
    {
      ScopedAllowBlockingForLynxtron allow_blocking;
      base::PathService::Override(DIR_APP_LOGS, custom_path.value());
    }
  } else {
    NSString* bundle_name =
        [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];
    NSString* logs_path =
        [NSString stringWithFormat:@"Library/Logs/%@", bundle_name];
    NSString* library_path =
        [NSHomeDirectory() stringByAppendingPathComponent:logs_path];
    {
      ScopedAllowBlockingForLynxtron allow_blocking;
      base::PathService::Override(DIR_APP_LOGS,
                                  base::FilePath([library_path UTF8String]));
    }
  }
}

void App::SetActivationPolicy(gin_helper::ErrorThrower thrower,
                              const std::string& policy) {
  NSApplicationActivationPolicy activation_policy;
  if (policy == "accessory") {
    activation_policy = NSApplicationActivationPolicyAccessory;
  } else if (policy == "prohibited") {
    activation_policy = NSApplicationActivationPolicyProhibited;
  } else if (policy == "regular") {
    activation_policy = NSApplicationActivationPolicyRegular;
  } else {
    thrower.ThrowError("Invalid activation policy: must be one of 'regular', "
                       "'accessory', or 'prohibited'");
    return;
  }

  [NSApp setActivationPolicy:activation_policy];
}

v8::Local<v8::Promise> App::GetFileIcon(const base::FilePath& path,
                                        gin::Arguments* args) {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  gin_helper::Promise<gfx::Image> promise(isolate);
  v8::Local<v8::Promise> handle = promise.GetHandle();

  gin_helper::Dictionary options;
  std::string size_string = "normal";
  if (args->GetNext(&options)) {
    options.Get("size", &size_string);
  }
  IconManager::IconSize size = IconManager::IconSize::kNormal;
  if (!IconManager::ParseIconSize(size_string, &size)) {
    promise.RejectWithErrorMessage(
        "size must be one of 'small', 'normal', 'large'");
    return handle;
  }

  base::FilePath normalized_path = path.NormalizePathSeparators();
  auto* icon_manager = MainParts::Get()->GetIconManager();
  gfx::Image* icon =
      icon_manager->LookupIconFromFilepath(normalized_path, size, 1.0f);
  if (icon) {
    promise.Resolve(*icon);
    return handle;
  }
  icon_manager->LoadIcon(
      normalized_path, size, 1.0f,
      base::BindOnce(&OnIconDataAvailable, std::move(promise)),
      &cancelable_task_tracker_);
  return handle;
}

// bool App::IsRunningUnderRosettaTranslation() const {
//   node::Environment* env =
//       node::Environment::GetCurrent(lynxtron::JavascriptEnvironment::GetIsolate());

//   EmitWarning(env,
//               "The app.runningUnderRosettaTranslation API is deprecated, use
//               " "app.runningUnderARM64Translation instead.", "electron");
//   return IsRunningUnderARM64Translation();
// }

bool App::IsRunningUnderARM64Translation() const {
  int proc_translated = 0;
  size_t size = sizeof(proc_translated);
  if (sysctlbyname("sysctl.proc_translated", &proc_translated, &size, NULL,
                   0) == -1) {
    return false;
  }
  return proc_translated == 1;
}

}  // namespace lynxtron::api
