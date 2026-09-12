// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/ui/file_dialog.h"

#include <filemanagement/fileshare/oh_file_share.h>

#include <future>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/promise.h"
#include "v8/include/v8-isolate.h"

namespace file_dialog {

// HarmonyOS implementation of the file_dialog free functions.
//
// Flow (mirrors Electron OHOS shell/browser/ui/file_dialog_ohos.cc):
//   ShowOpenDialog (JS thread)
//     -> ThreadPool PostTaskAndReplyWithResult
//     -> ShowOpenDialogAdapter (background thread)
//        -> g_show_open_dialog_handler (injected by lynxtron_napi_bridge.cc
//           via dlsym, same pattern as LynxtronSetOpenExternalHandler)
//           -> TSFN -> ArkTS FileDialogBridge -> DocumentViewPicker.select()
//        -> blocks on std::future until ArkTS resolves via
//           lynxtron.resolveShowOpenDialog(id, uris, paths, canceled)
//
// THREAD AFFINITY: OH_FileShare_PersistPermission runs on the ETS thread
// (inside the resolve callback, which is ResolveShowOpenDialog invoked from
// ArkTS).  It is a plain C API and works fine.
//
// DO NOT call OH_FileUri_GetPathFromUri from liblynxtron.so: its internal
// `new FileUri` allocation is intercepted by Chromium's allocator shim
// (__wrap_malloc / partition_alloc), which crashes inside FileUri::GetRealPath
// (SIGSEGV, device-verified on both the thread pool and the ETS thread).
// URI -> path conversion therefore happens on the ArkTS side with
// `new fileUri.FileUri(uri).path` (same as Electron's FilePickerAdapter
// dirFilter).  ArkTS passes REAL PATHS to resolveShowOpenDialog.
//
// The handler is a plain C function-pointer pair so it can cross the
// liblynxtron.so / liblynxtron_napi.so boundary without C++ ABI coupling.

using DialogResultCallback =
    void (*)(void* user_data, const char* const* uris, size_t uri_count,
             const char* const* paths, size_t path_count, bool canceled);
using ShowOpenDialogHandler = void (*)(const char* settings_json,
                                       DialogResultCallback callback,
                                       void* user_data);

// Save dialog: single result (uri + converted path).  Same thread rules as
// open: ArkTS converts URI->path, C++ only persists the URI.
using SaveDialogResultCallback =
    void (*)(void* user_data, const char* uri, const char* path,
             bool canceled);
using ShowSaveDialogHandler = void (*)(const char* settings_json,
                                       SaveDialogResultCallback callback,
                                       void* user_data);

// Injected by lynxtron_napi_bridge.cc (dlsym on LynxtronSetShowOpenDialogHandler).
ShowOpenDialogHandler g_show_open_dialog_handler = nullptr;
ShowSaveDialogHandler g_show_save_dialog_handler = nullptr;

namespace {

struct OpenDialogResult {
  std::vector<std::string> file_paths;
  bool canceled = true;
};

// Persist file access for the picked URIs so the app can keep reading them
// after a restart.  Mirrors Electron's DialogAdapter::FileAccessPersist
// (oh_file_share.h).  Runs on the ETS thread (inside the resolve callback).
void FileAccessPersist(const std::vector<std::string>& uris) {
  if (uris.empty()) {
    return;
  }
  std::vector<FileShare_PolicyInfo> policies;
  policies.reserve(uris.size());
  for (const auto& uri : uris) {
    FileShare_PolicyInfo policy{};
    policy.uri = const_cast<char*>(uri.c_str());
    policy.length = static_cast<unsigned int>(uri.length());
    policy.operationMode = FileShare_OperationMode::READ_MODE |
                           FileShare_OperationMode::WRITE_MODE;
    policies.push_back(policy);
  }
  FileShare_PolicyErrorResult* result = nullptr;
  uint32_t result_num = 0;
  FileManagement_ErrCode ret = OH_FileShare_PersistPermission(
      policies.data(), static_cast<unsigned int>(policies.size()), &result,
      &result_num);
  if (ret != FileManagement_ErrCode::ERR_OK) {
    LOG(ERROR) << __func__ << " OH_FileShare_PersistPermission failed, ret="
               << ret;
  }
  OH_FileShare_ReleasePolicyErrorResult(result, result_num);
}

// Result callback invoked by the NAPI bridge on the ETS thread when the
// ArkTS picker finishes.  user_data is the std::promise<OpenDialogResult>*
// created by ShowOpenDialogAdapter.
//
// ArkTS passes BOTH the raw picker URIs (for FileAccessPersist — the API
// requires file://docs URIs, not paths) and the converted real paths (for
// the JS-visible filePaths result).  FileAccessPersist runs here, on the
// ETS thread.
void OnOpenDialogResult(void* user_data, const char* const* uris,
                        size_t uri_count, const char* const* paths,
                        size_t path_count, bool canceled) {
  auto* promise = static_cast<std::promise<OpenDialogResult>*>(user_data);
  if (!promise) {
    return;
  }
  OpenDialogResult result;
  result.canceled = canceled;
  for (size_t i = 0; i < path_count; ++i) {
    if (paths[i]) {
      result.file_paths.emplace_back(paths[i]);
    }
  }
  if (!canceled) {
    std::vector<std::string> uris_vec;
    for (size_t i = 0; i < uri_count; ++i) {
      if (uris[i]) {
        uris_vec.emplace_back(uris[i]);
      }
    }
    FileAccessPersist(uris_vec);
  }
  promise->set_value(std::move(result));
}

void ConvertFilters(const Filters& filters, base::Value::List& filters_list) {
  for (auto& filter : filters) {
    base::Value::Dict file_filter;
    base::Value::List extensions_list;
    for (auto& extension : filter.second) {
      extensions_list.Append(extension);
    }
    file_filter.Set("name", filter.first);
    file_filter.Set("extensions", std::move(extensions_list));
    filters_list.Append(std::move(file_filter));
  }
}

// Serializes DialogSettings into the JSON shape the ArkTS FileDialogBridge
// expects.  Field names match Electron OHOS exactly; default_path is passed
// through verbatim and resolved on the ArkTS side (no OHOS NDK dependency).
std::string SettingsConvertToJsonStr(const DialogSettings& settings) {
  // Lynxtron does not forward a parent window to the OHOS picker yet;
  // keep parent_id for API-shape parity with Electron.
  const int parent_id = -1;

  base::Value::List filters;
  ConvertFilters(settings.filters, filters);

  const bool is_open_directory =
      settings.properties & OPEN_DIALOG_OPEN_DIRECTORY;
  const bool is_open_mixed =
      (settings.properties & OPEN_DIALOG_OPEN_FILE) && is_open_directory;
  const bool is_multi_selections =
      settings.properties & OPEN_DIALOG_MULTI_SELECTIONS;

  base::Value::Dict settings_dict;
  settings_dict.Set("parent_id", parent_id);
  settings_dict.Set("title", settings.title);
  settings_dict.Set("message", settings.message);
  settings_dict.Set("button_label", settings.button_label);
  settings_dict.Set("default_path", settings.default_path.value());
  settings_dict.Set("filters", std::move(filters));
  settings_dict.Set("properties_open_directory", is_open_directory);
  settings_dict.Set("properties_open_mixed", is_open_mixed);
  settings_dict.Set("properties_multi_selections", is_multi_selections);

  std::string settings_json;
  base::JSONWriter::Write(settings_dict, &settings_json);
  return settings_json;
}

OpenDialogResult ShowOpenDialogAdapter(const DialogSettings& settings) {
  OpenDialogResult result;  // canceled = true by default
  if (!g_show_open_dialog_handler) {
    return result;
  }

  const std::string settings_json = SettingsConvertToJsonStr(settings);
  auto promise = std::make_shared<std::promise<OpenDialogResult>>();
  g_show_open_dialog_handler(settings_json.c_str(), &OnOpenDialogResult,
                             promise.get());
  // Blocks until ArkTS resolves the picker via resolveShowOpenDialog; the
  // ETS-side callback already persisted access.  Paths arrive pre-converted.
  result = promise->get_future().get();
  return result;
}

struct SaveDialogResult {
  std::string file_path;
  bool canceled = true;
};

// Result callback for the save dialog, invoked by the NAPI bridge on the ETS
// thread.  Same contract as OnOpenDialogResult: persist the raw URI (never the
// path), return the pre-converted path to JS.
void OnSaveDialogResult(void* user_data, const char* uri, const char* path,
                        bool canceled) {
  auto* promise = static_cast<std::promise<SaveDialogResult>*>(user_data);
  if (!promise) {
    return;
  }
  SaveDialogResult result;
  result.canceled = canceled;
  if (path) {
    result.file_path = path;
  }
  if (!canceled && uri) {
    FileAccessPersist({std::string(uri)});
  }
  promise->set_value(std::move(result));
}

SaveDialogResult ShowSaveDialogAdapter(const DialogSettings& settings) {
  SaveDialogResult result;  // canceled = true by default
  if (!g_show_save_dialog_handler) {
    return result;
  }

  const std::string settings_json = SettingsConvertToJsonStr(settings);
  auto promise = std::make_shared<std::promise<SaveDialogResult>>();
  g_show_save_dialog_handler(settings_json.c_str(), &OnSaveDialogResult,
                             promise.get());
  result = promise->get_future().get();
  return result;
}

}  // namespace

extern "C" __attribute__((visibility("default")))
void LynxtronSetShowOpenDialogHandler(ShowOpenDialogHandler handler) {
  g_show_open_dialog_handler = handler;
}

extern "C" __attribute__((visibility("default")))
void LynxtronSetShowSaveDialogHandler(ShowSaveDialogHandler handler) {
  g_show_save_dialog_handler = handler;
}

DialogSettings::DialogSettings() = default;
DialogSettings::DialogSettings(const DialogSettings&) = default;
DialogSettings::~DialogSettings() = default;

bool ShowOpenDialogSync(const DialogSettings& settings,
                        std::vector<base::FilePath>* paths) {
  // Not implemented on HarmonyOS: the DocumentViewPicker is async-only and a
  // sync variant would block the JS thread.  Return false (canceled).
  return false;
}

void ShowOpenDialog(const DialogSettings& settings,
                    gin_helper::Promise<gin_helper::Dictionary> promise) {
  auto done = [](gin_helper::Promise<gin_helper::Dictionary> promise,
                 OpenDialogResult result) {
    v8::HandleScope handle_scope(promise.isolate());
    gin_helper::Dictionary dict =
        gin_helper::Dictionary::CreateEmpty(promise.isolate());
    dict.Set("canceled", result.canceled);
    dict.Set("filePaths", result.file_paths);
    promise.Resolve(dict);
  };

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ShowOpenDialogAdapter, settings),
      base::BindOnce(done, std::move(promise)));
}

std::optional<base::FilePath> ShowSaveDialogSync(
    const DialogSettings& settings) {
  // Not implemented on HarmonyOS: the DocumentViewPicker is async-only and a
  // sync variant would block the JS thread.  Return nullopt (canceled).
  return std::nullopt;
}

void ShowSaveDialog(const DialogSettings& settings,
                    gin_helper::Promise<gin_helper::Dictionary> promise) {
  auto done = [](gin_helper::Promise<gin_helper::Dictionary> promise,
                 SaveDialogResult result) {
    v8::HandleScope handle_scope(promise.isolate());
    gin_helper::Dictionary dict =
        gin_helper::Dictionary::CreateEmpty(promise.isolate());
    dict.Set("canceled", result.canceled);
    dict.Set("filePath", result.file_path);
    promise.Resolve(dict);
  };

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ShowSaveDialogAdapter, settings),
      base::BindOnce(done, std::move(promise)));
}

}  // namespace file_dialog
