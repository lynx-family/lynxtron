// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/api/api_app.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "gin/arguments.h"
#include "shell/app/javascript_environment.h"
#include "shell/app/main_parts.h"
#include "shell/app/relauncher.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "shell/common/gin_converters/login_item_settings_converter.h"
#include "shell/common/gin_converters/value_converter.h"
#include "shell/common/gin_helper/constructible.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/error_thrower.h"
#include "shell/common/gin_helper/handle.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/gin_helper/pinnable.h"
#include "shell/common/global_thread.h"
#include "shell/common/lynxtron_command_line.h"
#include "shell/common/lynxtron_paths.h"
#include "shell/common/node_includes.h"
#include "shell/common/options_switches.h"
#include "shell/common/platform_util.h"
#include "shell/common/thread_restrictions.h"
#include "shell/common/v8_util.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "shell/api/ui/win/jump_list.h"
#endif

#if BUILDFLAG(IS_MAC)
#include <CoreFoundation/CoreFoundation.h>

#include "shell/ui/cocoa/electron_bundle_mover.h"
#endif

using lynxtron::Application;

namespace gin {

#if BUILDFLAG(IS_WIN)
template <>
struct Converter<Application::UserTask> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     Application::UserTask* out) {
    gin_helper::Dictionary dict;
    if (!ConvertFromV8(isolate, val, &dict)) {
      return false;
    }
    if (!dict.Get("program", &(out->program)) ||
        !dict.Get("title", &(out->title))) {
      return false;
    }
    if (dict.Get("iconPath", &(out->icon_path)) &&
        !dict.Get("iconIndex", &(out->icon_index))) {
      return false;
    }
    dict.Get("arguments", &(out->arguments));
    dict.Get("description", &(out->description));
    dict.Get("workingDirectory", &(out->working_dir));
    return true;
  }
};

using lynxtron::JumpListCategory;
using lynxtron::JumpListItem;
using lynxtron::JumpListResult;

template <>
struct Converter<JumpListItem::Type> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     JumpListItem::Type* out) {
    std::string item_type;
    if (!ConvertFromV8(isolate, val, &item_type)) {
      return false;
    }

    if (item_type == "task") {
      *out = JumpListItem::Type::kTask;
    } else if (item_type == "separator") {
      *out = JumpListItem::Type::kSeparator;
    } else if (item_type == "file") {
      *out = JumpListItem::Type::kFile;
    } else {
      return false;
    }

    return true;
  }

  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   JumpListItem::Type val) {
    std::string item_type;
    switch (val) {
      case JumpListItem::Type::kTask:
        item_type = "task";
        break;

      case JumpListItem::Type::kSeparator:
        item_type = "separator";
        break;

      case JumpListItem::Type::kFile:
        item_type = "file";
        break;
    }
    return gin::ConvertToV8(isolate, item_type);
  }
};

template <>
struct Converter<JumpListItem> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     JumpListItem* out) {
    gin_helper::Dictionary dict;
    if (!ConvertFromV8(isolate, val, &dict)) {
      return false;
    }

    if (!dict.Get("type", &(out->type))) {
      return false;
    }

    switch (out->type) {
      case JumpListItem::Type::kTask:
        if (!dict.Get("program", &(out->path)) ||
            !dict.Get("title", &(out->title))) {
          return false;
        }

        if (dict.Get("iconPath", &(out->icon_path)) &&
            !dict.Get("iconIndex", &(out->icon_index))) {
          return false;
        }

        dict.Get("args", &(out->arguments));
        dict.Get("description", &(out->description));
        dict.Get("workingDirectory", &(out->working_dir));
        return true;

      case JumpListItem::Type::kSeparator:
        return true;

      case JumpListItem::Type::kFile:
        return dict.Get("path", &(out->path));
    }

    assert(false);
    return false;
  }

  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const JumpListItem& val) {
    gin_helper::Dictionary dict = gin::Dictionary::CreateEmpty(isolate);
    dict.Set("type", val.type);

    switch (val.type) {
      case JumpListItem::Type::kTask:
        dict.Set("program", val.path);
        dict.Set("args", val.arguments);
        dict.Set("title", val.title);
        dict.Set("iconPath", val.icon_path);
        dict.Set("iconIndex", val.icon_index);
        dict.Set("description", val.description);
        dict.Set("workingDirectory", val.working_dir);
        break;

      case JumpListItem::Type::kSeparator:
        break;

      case JumpListItem::Type::kFile:
        dict.Set("path", val.path);
        break;
    }
    return dict.GetHandle();
  }
};

template <>
struct Converter<JumpListCategory::Type> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     JumpListCategory::Type* out) {
    std::string category_type;
    if (!ConvertFromV8(isolate, val, &category_type)) {
      return false;
    }

    if (category_type == "tasks") {
      *out = JumpListCategory::Type::kTasks;
    } else if (category_type == "frequent") {
      *out = JumpListCategory::Type::kFrequent;
    } else if (category_type == "recent") {
      *out = JumpListCategory::Type::kRecent;
    } else if (category_type == "custom") {
      *out = JumpListCategory::Type::kCustom;
    } else {
      return false;
    }

    return true;
  }

  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   JumpListCategory::Type val) {
    std::string category_type;
    switch (val) {
      case JumpListCategory::Type::kTasks:
        category_type = "tasks";
        break;

      case JumpListCategory::Type::kFrequent:
        category_type = "frequent";
        break;

      case JumpListCategory::Type::kRecent:
        category_type = "recent";
        break;

      case JumpListCategory::Type::kCustom:
        category_type = "custom";
        break;
    }
    return gin::ConvertToV8(isolate, category_type);
  }
};

template <>
struct Converter<JumpListCategory> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     JumpListCategory* out) {
    gin_helper::Dictionary dict;
    if (!ConvertFromV8(isolate, val, &dict)) {
      return false;
    }

    if (dict.Get("name", &(out->name)) && out->name.empty()) {
      return false;
    }

    if (!dict.Get("type", &(out->type))) {
      if (out->name.empty()) {
        out->type = JumpListCategory::Type::kTasks;
      } else {
        out->type = JumpListCategory::Type::kCustom;
      }
    }

    if ((out->type == JumpListCategory::Type::kTasks) ||
        (out->type == JumpListCategory::Type::kCustom)) {
      if (!dict.Get("items", &(out->items))) {
        return false;
      }
    }

    return true;
  }
};

// static
template <>
struct Converter<JumpListResult> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate, JumpListResult val) {
    std::string result_code;
    switch (val) {
      case JumpListResult::kSuccess:
        result_code = "ok";
        break;

      case JumpListResult::kArgumentError:
        result_code = "argumentError";
        break;

      case JumpListResult::kGenericError:
        result_code = "error";
        break;

      case JumpListResult::kCustomCategorySeparatorError:
        result_code = "invalidSeparatorError";
        break;

      case JumpListResult::kMissingFileTypeRegistrationError:
        result_code = "fileTypeRegistrationError";
        break;

      case JumpListResult::kCustomCategoryAccessDeniedError:
        result_code = "customCategoryAccessDeniedError";
        break;
    }
    return ConvertToV8(isolate, result_code);
  }
};
#endif

}  // namespace gin

namespace lynxtron::api {

gin::DeprecatedWrapperInfo App::kWrapperInfo = {gin::kEmbedderNativeGin};

namespace {

// IconLoader::IconSize GetIconSizeByString(const std::string& size) {
//   if (size == "small") {
//     return IconLoader::IconSize::SMALL;
//   } else if (size == "large") {
//     return IconLoader::IconSize::LARGE;
//   }
//   return IconLoader::IconSize::NORMAL;
// }

// Return the path constant from string.
int GetPathConstant(const std::string& name) {
  if (name == "appData") {
    return lynxtron::DIR_APP_DATA;
  } else if (name == "userData") {
    return lynxtron::DIR_USER_DATA;
  } else if (name == "cache")
#if BUILDFLAG(IS_POSIX)
    return base::DIR_CACHE;
#else
    return base::DIR_ROAMING_APP_DATA;
#endif
  else if (name == "userCache") {
    return DIR_USER_CACHE;
  } else if (name == "logs") {
    return DIR_APP_LOGS;
  } else if (name == "crashDumps") {
    return DIR_CRASH_DUMPS;
  } else if (name == "home") {
    return base::DIR_HOME;
  } else if (name == "temp") {
    return base::DIR_TEMP;
  } else if (name == "userDesktop" || name == "desktop") {
    return base::DIR_USER_DESKTOP;
  } else if (name == "exe") {
    return base::FILE_EXE;
  } else if (name == "module") {
    return base::FILE_MODULE;
  } else if (name == "documents") {
    return DIR_USER_DOCUMENTS;
  } else if (name == "downloads") {
    return DIR_DEFAULT_DOWNLOADS;
  } else if (name == "music") {
    return DIR_USER_MUSIC;
  } else if (name == "pictures") {
    return DIR_USER_PICTURES;
  } else if (name == "videos") {
    return DIR_USER_VIDEOS;
  }
#if BUILDFLAG(IS_WIN)
  else if (name == "recent") {
    return lynxtron::DIR_RECENT;
  }
#endif
  else {
    return -1;
  }
}

bool NotificationCallbackWrapper(
    const base::RepeatingCallback<
        void(base::CommandLine command_line,
             const base::FilePath& current_directory,
             const std::vector<uint8_t> additional_data)>& callback,
    base::CommandLine cmd,
    const base::FilePath& cwd,
    const std::vector<uint8_t> additional_data) {
  // Make sure the callback is called after app gets ready.
  if (Application::Get()->is_ready()) {
    callback.Run(std::move(cmd), cwd, std::move(additional_data));
  } else {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner(
        base::SingleThreadTaskRunner::GetCurrentDefault());

    // Make a copy of the span so that the data isn't lost.
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(base::IgnoreResult(callback), std::move(cmd),
                                  cwd, std::move(additional_data)));
  }
  // ProcessSingleton needs to know whether current process is quitting.
  return !Application::Get()->is_shutting_down();
}

// TODO(Guo Xi): icon
// void OnIconDataAvailable(gin_helper::Promise<gfx::Image> promise,
//                          gfx::Image icon) {
//   if (!icon.IsEmpty()) {
//     promise.Resolve(icon);
//   } else {
//     promise.RejectWithErrorMessage("Failed to get file icon.");
//   }
// }

struct ProcessMemoryInfo {
  size_t working_set_size = 0;
  size_t peak_working_set_size = 0;
#if BUILDFLAG(IS_WIN)
  size_t private_bytes = 0;
#endif
};

#if BUILDFLAG(IS_WIN)

ProcessMemoryInfo GetMemoryInfo() {
  ProcessMemoryInfo result;

  // PROCESS_MEMORY_COUNTERS_EX info = {};
  // if (::GetProcessMemoryInfo(process.Handle(),
  //                            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&info),
  //                            sizeof(info))) {
  //   result.working_set_size = info.WorkingSetSize;
  //   result.peak_working_set_size = info.PeakWorkingSetSize;
  //   result.private_bytes = info.PrivateUsage;
  // }

  return result;
}

#elif BUILDFLAG(IS_MAC)

std::optional<mach_task_basic_info_data_t> GetTaskInfo(mach_port_t task) {
  if (task == MACH_PORT_NULL) {
    return std::nullopt;
  }
  mach_task_basic_info_data_t info = {};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  kern_return_t kr = task_info(task, MACH_TASK_BASIC_INFO,
                               reinterpret_cast<task_info_t>(&info), &count);
  return (kr == KERN_SUCCESS) ? std::make_optional(info) : std::nullopt;
}

ProcessMemoryInfo GetMemoryInfo() {
  ProcessMemoryInfo result;

  if (auto info = GetTaskInfo(mach_task_self())) {
    result.working_set_size = info->resident_size;
    result.peak_working_set_size = info->resident_size_max;
  }

  return result;
}

#endif

}  // namespace

App::App() {
  Application::Get()->AddObserver(this);
  // auto process_metric = std::make_unique<electron::ProcessMetric>(
  //     1 /*content::PROCESS_TYPE_BROWSER*/, base::GetCurrentProcessHandle(),
  //     base::ProcessMetrics::CreateCurrentProcessMetrics());
  // app_metric_ = std::move(process_metric);
}

App::~App() {
  Application::Get()->RemoveObserver(this);
}

void App::OnBeforeQuit(bool* prevent_default) {
  if (Emit("before-quit")) {
    *prevent_default = true;
  }
}

void App::OnWillQuit(bool* prevent_default) {
  if (Emit("will-quit")) {
    *prevent_default = true;
  }
}

void App::OnWindowAllClosed() {
  Emit("window-all-closed");
}

void App::OnQuit() {
  const int exitCode = lynxtron::MainParts::Get()->GetExitCode();
  Emit("quit", exitCode);
  if (process_singleton_) {
    process_singleton_->Cleanup();
    process_singleton_.reset();
  }
}

void App::OnOpenFile(bool* prevent_default, const std::string& file_path) {
  if (Emit("open-file", file_path)) {
    *prevent_default = true;
  }
}

void App::OnOpenURL(const std::string& url) {
  Emit("open-url", url);
}

void App::OnActivate(bool has_visible_windows) {
  Emit("activate", has_visible_windows);
}

void App::OnWillFinishLaunching() {
  Emit("will-finish-launching");
}

void App::OnFinishLaunching(base::Value::Dict launch_info) {
  Emit("ready", std::move(launch_info));
}

void App::OnPreMainMessageLoopRun() {
  if (process_singleton_ && watch_singleton_socket_on_ready_) {
    process_singleton_->StartWatching();
    watch_singleton_socket_on_ready_ = false;
  }
}

#if BUILDFLAG(IS_MAC)
void App::OnWillContinueUserActivity(bool* prevent_default,
                                     const std::string& type) {
  if (Emit("will-continue-activity", type)) {
    *prevent_default = true;
  }
}

void App::OnDidFailToContinueUserActivity(const std::string& type,
                                          const std::string& error) {
  Emit("continue-activity-error", type, error);
}

void App::OnContinueUserActivity(bool* prevent_default,
                                 const std::string& type,
                                 base::Value::Dict user_info,
                                 base::Value::Dict details) {
  if (Emit("continue-activity", type, base::Value(std::move(user_info)),
           base::Value(std::move(details)))) {
    *prevent_default = true;
  }
}

void App::OnUserActivityWasContinued(const std::string& type,
                                     base::Value::Dict user_info) {
  Emit("activity-was-continued", type, base::Value(std::move(user_info)));
}

void App::OnUpdateUserActivityState(bool* prevent_default,
                                    const std::string& type,
                                    base::Value::Dict user_info) {
  if (Emit("update-activity-state", type, base::Value(std::move(user_info)))) {
    *prevent_default = true;
  }
}

void App::OnNewWindowForTab() {
  Emit("new-window-for-tab");
}

void App::OnDidBecomeActive() {
  Emit("did-become-active");
}

void App::OnDidResignActive() {
  Emit("did-resign-active");
}

#endif

base::FilePath App::GetAppPath() const {
  return app_path_;
}

void App::SetAppPath(const base::FilePath& app_path) {
  app_path_ = app_path;
}

void App::OnRunAsElevatedComplete(gin_helper::Promise<bool> promise,
                                  bool success,
                                  int32_t code) {
  v8::Isolate* isolate = lynxtron::JavascriptEnvironment::GetIsolate();
  if (!isolate) {
    return;
  }
  v8::HandleScope handle_scope(isolate);
  if (success) {
    promise.Resolve(true);
  } else {
    promise.RejectWithErrorMessage("Elevation service launch failed, code=" +
                                   std::to_string(code));
  }
}

#if !BUILDFLAG(IS_MAC)
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
    base::FilePath path;
    if (base::PathService::Get(DIR_USER_DATA, &path)) {
      path = path.Append(base::FilePath::FromUTF8Unsafe("logs"));
      {
        ScopedAllowBlockingForLynxtron allow_blocking;
        base::PathService::Override(DIR_APP_LOGS, path);
      }
    }
  }
}
#endif

// static
bool App::IsPackaged() {
  auto env = base::Environment::Create();
  if (env->HasVar("ELECTRON_FORCE_IS_PACKAGED")) {
    return true;
  }

  base::FilePath exe_path;
  base::PathService::Get(base::FILE_EXE, &exe_path);
  base::FilePath::StringType base_name =
      base::ToLowerASCII(exe_path.BaseName().value());

#if BUILDFLAG(IS_WIN)
  return base_name != FILE_PATH_LITERAL("electron.exe");
#else
  return base_name != FILE_PATH_LITERAL("electron");
#endif
}

base::FilePath App::GetPath(gin_helper::ErrorThrower thrower,
                            const std::string& name) {
  base::FilePath path;

  int key = GetPathConstant(name);
  if (key < 0 || !base::PathService::Get(key, &path)) {
    thrower.ThrowError("Failed to get '" + name + "' path");
  }

  return path;
}

void App::SetPath(gin_helper::ErrorThrower thrower,
                  const std::string& name,
                  const base::FilePath& path) {
  if (!path.IsAbsolute()) {
    thrower.ThrowError("Path must be absolute");
    return;
  }

  int key = GetPathConstant(name);
  if (key < 0 ||
      !base::PathService::OverrideAndCreateIfNeeded(
          key, path, /* is_absolute = */ true, /* create = */ false)) {
    thrower.ThrowError("Failed to set path");
  }
}

std::string App::GetLocaleCountryCode() {
  std::string region;
#if BUILDFLAG(IS_WIN)
  WCHAR locale_name[LOCALE_NAME_MAX_LENGTH] = {0};

  if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO3166CTRYNAME,
                      (LPWSTR)&locale_name,
                      sizeof(locale_name) / sizeof(WCHAR)) ||
      GetLocaleInfoEx(LOCALE_NAME_SYSTEM_DEFAULT, LOCALE_SISO3166CTRYNAME,
                      (LPWSTR)&locale_name,
                      sizeof(locale_name) / sizeof(WCHAR))) {
    base::WideToUTF8(locale_name, wcslen(locale_name), &region);
  }
#elif BUILDFLAG(IS_MAC)
  CFLocaleRef locale = CFLocaleCopyCurrent();
  CFStringRef value = CFStringRef(
      static_cast<CFTypeRef>(CFLocaleGetValue(locale, kCFLocaleCountryCode)));
  if (value != nil) {
    char temporaryCString[3];
    const CFIndex kCStringSize = sizeof(temporaryCString);
    if (CFStringGetCString(value, temporaryCString, kCStringSize,
                           kCFStringEncodingUTF8)) {
      region = temporaryCString;
    }
  }
#else
  const char* locale_ptr = setlocale(LC_TIME, nullptr);
  if (!locale_ptr) {
    locale_ptr = setlocale(LC_NUMERIC, nullptr);
  }
  if (locale_ptr) {
    std::string locale = locale_ptr;
    std::string::size_type rpos = locale.find('.');
    if (rpos != std::string::npos) {
      locale = locale.substr(0, rpos);
    }
    rpos = locale.find('_');
    if (rpos != std::string::npos && rpos + 1 < locale.size()) {
      region = locale.substr(rpos + 1);
    }
  }
#endif
  return region.size() == 2 ? region : std::string();
}

void App::OnSecondInstance(base::CommandLine cmd,
                           const base::FilePath& cwd,
                           const std::vector<uint8_t> additional_data) {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> data_value =
      DeserializeV8Value(isolate, std::move(additional_data));
  Emit("second-instance", cmd.argv(), cwd, data_value);
}

bool App::HasSingleInstanceLock() const {
  if (process_singleton_) {
    return true;
  }
  return false;
}

bool App::RequestSingleInstanceLock(gin::Arguments* args) {
  if (HasSingleInstanceLock()) {
    return true;
  }

  base::FilePath user_dir;
  // TODO(Guo Xi): review DIR_USER_DATA
  base::PathService::Get(lynxtron::DIR_USER_DATA, &user_dir);
  // The user_dir may not have been created yet.
  base::CreateDirectoryAndGetError(user_dir, nullptr);

  auto cb = base::BindRepeating(&App::OnSecondInstance, base::Unretained(this));

  v8::Local<v8::Value> additional_data_message;

  std::vector<uint8_t> serialized_additional_data;
  if (args->GetNext(&additional_data_message)) {
    if (!SerializeV8Value(JavascriptEnvironment::GetIsolate(),
                          additional_data_message,
                          serialized_additional_data)) {
      return false;
    }
  }

#if BUILDFLAG(IS_WIN)
  const std::string program_name = Application::Get()->GetName();

  process_singleton_ = std::make_unique<ProcessSingleton>(
      program_name, user_dir, serialized_additional_data, false,
      base::BindRepeating(NotificationCallbackWrapper, cb));

#else
  process_singleton_ = std::make_unique<ProcessSingleton>(
      user_dir, serialized_additional_data,
      base::BindRepeating(NotificationCallbackWrapper, cb));
#endif

  switch (process_singleton_->NotifyOtherProcessOrCreate()) {
    case ProcessSingleton::NotifyResult::PROCESS_NONE:
      if (GlobalThread::IsThreadInitialized(GlobalThread::IO)) {
        process_singleton_->StartWatching();
      } else {
        watch_singleton_socket_on_ready_ = true;
      }
      return true;
    case ProcessSingleton::NotifyResult::LOCK_ERROR:
    case ProcessSingleton::NotifyResult::PROFILE_IN_USE:
    case ProcessSingleton::NotifyResult::PROCESS_NOTIFIED: {
      process_singleton_.reset();
      return false;
    }
  }
}

void App::ReleaseSingleInstanceLock() {
  if (process_singleton_) {
    process_singleton_->Cleanup();
    process_singleton_.reset();
  }
}

bool App::Relaunch(gin::Arguments* js_args) {
  LOG(INFO) << "App::Relaunch";
  // Parse parameters.
  bool override_argv = false;
  base::FilePath exec_path;
  relauncher::StringVector args;

  gin_helper::Dictionary options;
  if (js_args->GetNext(&options)) {
    bool has_exec_path = options.Get("execPath", &exec_path);
    bool has_args = options.Get("args", &args);
    if (has_exec_path || has_args) {
      override_argv = true;
    }
  }

  if (!override_argv) {
    const relauncher::StringVector& argv = LynxtronCommandLine::argv();
    return relauncher::RelaunchApp(argv);
  }

  relauncher::StringVector argv;
  argv.reserve(1 + args.size());

  if (exec_path.empty()) {
    base::FilePath current_exe_path;
    base::PathService::Get(base::FILE_EXE, &current_exe_path);
    argv.push_back(current_exe_path.value());
  } else {
    argv.push_back(exec_path.value());
  }

  argv.insert(argv.end(), args.begin(), args.end());

  return relauncher::RelaunchApp(argv);
}

LoginItemSettings App::GetLoginItemSettings(gin::Arguments* args) {
  // TODO(Guo Xi):support LoginItemSettings
  // LoginItemSettings options;
  // args->GetNext(&options);
  // return Application::Get()->GetLoginItemSettings(options);
  return {};
}

#if defined(OS_WIN)
v8::Local<v8::Value> App::GetJumpListSettings() {
  JumpList jump_list(Application::Get()->GetAppUserModelID());

  int min_items = 10;
  std::vector<JumpListItem> removed_items;
  if (jump_list.Begin(&min_items, &removed_items)) {
    // We don't actually want to change anything, so abort the transaction.
    jump_list.Abort();
  } else {
    LOG(ERROR) << "Failed to begin Jump List transaction.";
  }

  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  gin_helper::Dictionary dict = gin::Dictionary::CreateEmpty(isolate);
  dict.Set("minItems", min_items);
  dict.Set("removedItems", gin::ConvertToV8(isolate, removed_items));
  return dict.GetHandle();
}

JumpListResult App::SetJumpList(v8::Local<v8::Value> val,
                                gin::Arguments* args) {
  std::vector<JumpListCategory> categories;
  bool delete_jump_list = val->IsNull();
  if (!delete_jump_list &&
      !gin::ConvertFromV8(args->isolate(), val, &categories)) {
    gin_helper::ErrorThrower(args->isolate())
        .ThrowError("Argument must be null or an array of categories");
    return JumpListResult::kArgumentError;
  }

  JumpList jump_list(Application::Get()->GetAppUserModelID());

  if (delete_jump_list) {
    return jump_list.Delete() ? JumpListResult::kSuccess
                              : JumpListResult::kGenericError;
  }

  // Start a transaction that updates the JumpList of this application.
  if (!jump_list.Begin()) {
    return JumpListResult::kGenericError;
  }

  JumpListResult result = jump_list.AppendCategories(categories);
  // AppendCategories may have failed to add some categories, but it's better
  // to have something than nothing so try to commit the changes anyway.
  if (!jump_list.Commit()) {
    LOG(ERROR) << "Failed to commit changes to custom Jump List.";
    // It's more useful to return the earlier error code that might give
    // some indication as to why the transaction actually failed, so don't
    // overwrite it with a "generic error" code here.
    if (result == JumpListResult::kSuccess) {
      result = JumpListResult::kGenericError;
    }
  }

  return result;
}

#endif  // defined(OS_WIN)

// TODO(Guo Xi): support GetFileIcon
// v8::Local<v8::Promise> App::GetFileIcon(const base::FilePath& path,
//                                         gin::Arguments* args) {
//   v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
//   gin_helper::Promise<gfx::Image> promise(isolate);
//   v8::Local<v8::Promise> handle = promise.GetHandle();
//   base::FilePath normalized_path = path.NormalizePathSeparators();

//   IconLoader::IconSize icon_size;
//   gin_helper::Dictionary options;
//   if (!args->GetNext(&options)) {
//     icon_size = IconLoader::IconSize::NORMAL;
//   } else {
//     std::string icon_size_string;
//     options.Get("size", &icon_size_string);
//     icon_size = GetIconSizeByString(icon_size_string);
//   }

//   // TODO(Guo Xi) : icon manager
//   // auto* icon_manager = ElectronMainParts::Get()->GetIconManager();
//   // gfx::Image* icon =
//   //     icon_manager->LookupIconFromFilepath(normalized_path,
//   icon_size, 1.0f);
//   // if (icon) {
//   //   promise.Resolve(*icon);
//   // } else {
//   //   icon_manager->LoadIcon(
//   //       normalized_path, icon_size, 1.0f,
//   //       base::BindOnce(&OnIconDataAvailable, std::move(promise)),
//   //       &cancelable_task_tracker_);
//   // }
//   return handle;
// }

gin_helper::Dictionary App::GetAppMetrics(v8::Isolate* isolate) {
  gin_helper::Dictionary result;
  int processor_count = base::SysInfo::NumberOfProcessors();
  auto pid_dict = gin_helper::Dictionary::CreateEmpty(isolate);
  auto cpu_dict = gin_helper::Dictionary::CreateEmpty(isolate);
  // Default usage percentage to 0 for compatibility
  double usagePercent = 0;
  auto metric = base::ProcessMetrics::CreateCurrentProcessMetrics();
  if (auto usage = metric->GetCumulativeCPUUsage(); usage.has_value()) {
    cpu_dict.Set("cumulativeCPUUsage", usage->InSecondsF());
    usagePercent = metric->GetPlatformIndependentCPUUsage(*usage);
  }
  cpu_dict.Set("percentCPUUsage", usagePercent / processor_count);
#if !BUILDFLAG(IS_WIN)
  cpu_dict.Set("idleWakeupsPerSecond", metric->GetIdleWakeupsPerSecond());
#else
  // Chrome's underlying process_metrics.cc will throw a non-fatal warning
  // that this method isn't implemented on Windows, so set it to 0 instead
  // of calling it
  cpu_dict.Set("idleWakeupsPerSecond", 0);
#endif
  pid_dict.Set("cpu", cpu_dict);
  auto current_process = base::Process(base::GetCurrentProcessHandle());
  pid_dict.Set("creationTime",
               current_process.CreationTime().InMillisecondsFSinceUnixEpoch());
  auto memory_info = GetMemoryInfo();
  auto memory_dict = gin_helper::Dictionary::CreateEmpty(isolate);
  memory_dict.Set("workingSetSize",
                  static_cast<double>(memory_info.working_set_size >> 10));
  memory_dict.Set("peakWorkingSetSize",
                  static_cast<double>(memory_info.peak_working_set_size >> 10));
#if BUILDFLAG(IS_WIN)
  memory_dict.Set("privateBytes",
                  static_cast<double>(memory_info.private_bytes >> 10));
#endif
  pid_dict.Set("memory", memory_dict);

  result = std::move(pid_dict);
  return result;
}

#if BUILDFLAG(IS_WIN)

bool App::IsRunningUnderARM64Translation() const {
  USHORT processMachine = 0;
  USHORT nativeMachine = 0;

  auto IsWow64Process2 = reinterpret_cast<decltype(&::IsWow64Process2)>(
      GetProcAddress(GetModuleHandle(L"kernel32.dll"), "IsWow64Process2"));

  if (IsWow64Process2 == nullptr) {
    return false;
  }

  if (!IsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine)) {
    return false;
  }

  return nativeMachine == IMAGE_FILE_MACHINE_ARM64;
}
#endif

#if BUILDFLAG(IS_MAC)
bool App::MoveToApplicationsFolder(gin_helper::ErrorThrower thrower,
                                   gin::Arguments* args) {
  return ElectronBundleMover::Move(thrower, args);
}

bool App::IsInApplicationsFolder() {
  return ElectronBundleMover::IsCurrentAppInApplicationsFolder();
}

int DockBounce(gin::Arguments* args) {
  int request_id = -1;
  std::string type = "informational";
  args->GetNext(&type);

  if (type == "critical") {
    request_id =
        Application::Get()->DockBounce(Application::BounceType::kCritical);
  } else if (type == "informational") {
    request_id =
        Application::Get()->DockBounce(Application::BounceType::kInformational);
  }
  return request_id;
}

// void DockSetMenu(electron::api::Menu* menu) {
//   Application::Get()->DockSetMenu(menu->model());
// }

v8::Local<v8::Value> App::GetDockAPI(v8::Isolate* isolate) {
  if (dock_.IsEmpty()) {
    // Initialize the Dock API, the methods are bound to "dock" which exists
    // for the lifetime of "app"
    auto application = base::Unretained(Application::Get());
    gin_helper::Dictionary dock_obj = gin::Dictionary::CreateEmpty(isolate);
    dock_obj.SetMethod("bounce", &DockBounce);
    dock_obj.SetMethod(
        "cancelBounce",
        base::BindRepeating(&Application::DockCancelBounce, application));
    dock_obj.SetMethod(
        "downloadFinished",
        base::BindRepeating(&Application::DockDownloadFinished, application));
    dock_obj.SetMethod(
        "setBadge",
        base::BindRepeating(&Application::DockSetBadgeText, application));
    dock_obj.SetMethod(
        "getBadge",
        base::BindRepeating(&Application::DockGetBadgeText, application));
    dock_obj.SetMethod(
        "hide", base::BindRepeating(&Application::DockHide, application));
    dock_obj.SetMethod(
        "show", base::BindRepeating(&Application::DockShow, application));
    dock_obj.SetMethod(
        "isVisible",
        base::BindRepeating(&Application::DockIsVisible, application));
    // dock_obj.SetMethod("setMenu", &DockSetMenu);
    // dock_obj.SetMethod("setIcon",
    //                    base::BindRepeating(&Application::DockSetIcon,
    //                    browser));

    dock_.Reset(isolate, dock_obj.GetHandle());
  }
  return v8::Local<v8::Value>::New(isolate, dock_);
}
#endif

// static
App* App::Get() {
  static base::NoDestructor<App> app;
  return app.get();
}

// static
gin_helper::Handle<App> App::Create(v8::Isolate* isolate) {
  return gin_helper::CreateHandle(isolate, Get());
}

gin::ObjectTemplateBuilder App::GetObjectTemplateBuilder(v8::Isolate* isolate) {
  auto application = base::Unretained(Application::Get());
  return gin_helper::EventEmitterMixin<App>::GetObjectTemplateBuilder(isolate)
      .SetMethod(
          "addRecentDocument",
          base::BindRepeating(&Application::AddRecentDocument, application))
      .SetMethod(
          "clearRecentDocuments",
          base::BindRepeating(&Application::ClearRecentDocuments, application))
      .SetMethod("exit", base::BindRepeating(&Application::Exit, application))
      .SetMethod("focus", base::BindRepeating(&Application::Focus, application))
      .SetMethod("getAppPath", &App::GetAppPath)
      .SetMethod("getApplicationInfoForProtocol",
                 base::BindRepeating(
                     &Application::GetApplicationInfoForProtocol, application))
      .SetMethod("getApplicationNameForProtocol",
                 base::BindRepeating(
                     &Application::GetApplicationNameForProtocol, application))
      .SetMethod("getLocaleCountryCode", &App::GetLocaleCountryCode)
      .SetMethod("getLoginItemSettings", &App::GetLoginItemSettings)
      .SetMethod("getName",
                 base::BindRepeating(&Application::GetName, application))
      .SetMethod("getPath", &App::GetPath)
      .SetMethod(
          "getRecentDocuments",
          base::BindRepeating(&Application::GetRecentDocuments, application))
      .SetMethod("getVersion",
                 base::BindRepeating(&Application::GetVersion, application))
      .SetMethod("hasSingleInstanceLock", &App::HasSingleInstanceLock)
      .SetMethod("isDefaultProtocolClient",
                 base::BindRepeating(&Application::IsDefaultProtocolClient,
                                     application))
      .SetMethod(
          "isEmojiPanelSupported",
          base::BindRepeating(&Application::IsEmojiPanelSupported, application))
      .SetProperty("isPackaged", &App::IsPackaged)
      .SetMethod("isReady",
                 base::BindRepeating(&Application::is_ready, application))
      .SetMethod("quit", base::BindRepeating(&Application::Quit, application))
      .SetMethod("relaunch", &App::Relaunch)
      .SetMethod("releaseSingleInstanceLock", &App::ReleaseSingleInstanceLock)
      .SetMethod("removeAsDefaultProtocolClient",
                 base::BindRepeating(
                     &Application::RemoveAsDefaultProtocolClient, application))
      .SetMethod("requestSingleInstanceLock", &App::RequestSingleInstanceLock)
      .SetMethod("setAppLogsPath", &App::SetAppLogsPath)
      .SetMethod("setAppPath", &App::SetAppPath)
      .SetMethod("setAsDefaultProtocolClient",
                 base::BindRepeating(&Application::SetAsDefaultProtocolClient,
                                     application))
      .SetMethod(
          "setLoginItemSettings",
          base::BindRepeating(&Application::SetLoginItemSettings, application))
      .SetMethod("setName",
                 base::BindRepeating(&Application::SetName, application))
      .SetMethod("setPath", &App::SetPath)
      .SetMethod("setVersion",
                 base::BindRepeating(&Application::SetVersion, application))
      .SetMethod("whenReady",
                 base::BindRepeating(&Application::WhenReady, application))
      // .SetMethod("getFileIcon", &App::GetFileIcon)
      .SetMethod("getAppMetrics", &App::GetAppMetrics)
      .SetMethod(
          "setAboutPanelOptions",
          base::BindRepeating(&Application::SetAboutPanelOptions, application))
      .SetMethod("showAboutPanel",
                 base::BindRepeating(&Application::ShowAboutPanel, application))
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
      .SetProperty("runningUnderARM64Translation",
                   &App::IsRunningUnderARM64Translation)
#endif
#if BUILDFLAG(IS_WIN)
      .SetMethod("getJumpListSettings", &App::GetJumpListSettings)
      .SetMethod(
          "setAppUserModelId",
          base::BindRepeating(&Application::SetAppUserModelID, application))
      .SetMethod("setJumpList", &App::SetJumpList)
      .SetMethod("setUserTasks",
                 base::BindRepeating(&Application::SetUserTasks, application))
#endif

#if BUILDFLAG(IS_MAC)
      .SetMethod("hide", base::BindRepeating(&Application::Hide, application))
      .SetMethod("isHidden",
                 base::BindRepeating(&Application::IsHidden, application))
      .SetMethod("show", base::BindRepeating(&Application::Show, application))
      .SetMethod("getCurrentActivityType",
                 base::BindRepeating(&Application::GetCurrentActivityType,
                                     application))
      .SetProperty("dock", &App::GetDockAPI)
      .SetMethod("isInApplicationsFolder", &App::IsInApplicationsFolder)
      .SetMethod("setActivationPolicy", &App::SetActivationPolicy)
      .SetMethod(
          "setUserActivity",
          base::BindRepeating(&Application::SetUserActivity, application))
#endif
  // #if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  //       .SetMethod("showEmojiPanel",
  //                  base::BindRepeating(&Application::ShowEmojiPanel,
  //                  browser))
  // #endif

#if defined(MAS_BUILD)
      .SetMethod("startAccessingSecurityScopedResource",
                 &App::StartAccessingSecurityScopedResource)
#endif
      ;
}

const char* App::GetTypeName() {
  return "App";
}

}  // namespace lynxtron::api

namespace {

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  gin_helper::Dictionary dict(isolate, exports);
  dict.Set("app", lynxtron::api::App::Create(isolate));
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(electron_browser_app, Initialize)
