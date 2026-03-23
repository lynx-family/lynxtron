// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <utility>
#include <vector>

#include "shell/api/ui/file_dialog.h"
#include "shell/api/ui/message_box.h"
#include "shell/app/javascript_environment.h"
#include "shell/common/gin_converters/file_dialog_converter.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "shell/common/gin_converters/message_box_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/promise.h"
#include "shell/common/node_includes.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-promise.h"

namespace {

int ShowMessageBoxSync(const lynxtron::MessageBoxSettings& settings) {
  return lynxtron::ShowMessageBoxSync(settings);
}

void ResolvePromiseObject(gin_helper::Promise<gin_helper::Dictionary> promise,
                          int result,
                          bool checkbox_checked) {
  v8::Isolate* isolate = promise.isolate();
  v8::HandleScope handle_scope(isolate);
  auto dict = gin_helper::Dictionary::CreateEmpty(isolate);

  dict.Set("response", result);
  dict.Set("checkboxChecked", checkbox_checked);

  promise.Resolve(dict);
}

v8::Local<v8::Promise> ShowMessageBox(
    const lynxtron::MessageBoxSettings& settings,
    gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  gin_helper::Promise<gin_helper::Dictionary> promise(isolate);
  v8::Local<v8::Promise> handle = promise.GetHandle();

  lynxtron::ShowMessageBox(
      settings, base::BindOnce(&ResolvePromiseObject, std::move(promise)));

  return handle;
}

void ShowOpenDialogSync(const file_dialog::DialogSettings& settings,
                        gin::Arguments* args) {
  std::vector<base::FilePath> paths;
  if (file_dialog::ShowOpenDialogSync(settings, &paths)) {
    args->Return(paths);
  }
}

v8::Local<v8::Promise> ShowOpenDialog(
    const file_dialog::DialogSettings& settings,
    gin::Arguments* args) {
  gin_helper::Promise<gin_helper::Dictionary> promise(args->isolate());
  v8::Local<v8::Promise> handle = promise.GetHandle();
  file_dialog::ShowOpenDialog(settings, std::move(promise));
  return handle;
}

void ShowSaveDialogSync(const file_dialog::DialogSettings& settings,
                        gin::Arguments* args) {
  if (const auto path = file_dialog::ShowSaveDialogSync(settings)) {
    args->Return(*path);
  }
}

v8::Local<v8::Promise> ShowSaveDialog(
    const file_dialog::DialogSettings& settings,
    gin::Arguments* args) {
  gin_helper::Promise<gin_helper::Dictionary> promise(args->isolate());
  v8::Local<v8::Promise> handle = promise.GetHandle();

  file_dialog::ShowSaveDialog(settings, std::move(promise));
  return handle;
}

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* const isolate = lynxtron::JavascriptEnvironment::GetIsolate();
  gin_helper::Dictionary dict{isolate, exports};
  dict.SetMethod("showMessageBoxSync", &ShowMessageBoxSync);
  dict.SetMethod("showMessageBox", &ShowMessageBox);
  dict.SetMethod("_closeMessageBox", &lynxtron::CloseMessageBox);
  dict.SetMethod("showErrorBox", &lynxtron::ShowErrorBox);
  dict.SetMethod("showOpenDialogSync", &ShowOpenDialogSync);
  dict.SetMethod("showOpenDialog", &ShowOpenDialog);
  dict.SetMethod("showSaveDialogSync", &ShowSaveDialogSync);
  dict.SetMethod("showSaveDialog", &ShowSaveDialog);
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_binding_dialog, Initialize)
