// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_API_API_APP_H_
#define LYNXTRON_SHELL_API_API_APP_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/task/cancelable_task_tracker.h"
#include "shell/api/event_emitter_mixin.h"
#include "shell/app/application.h"
#include "shell/app/application_observer.h"
#include "shell/common/gin_converters/gurl_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/error_thrower.h"
#include "shell/common/gin_helper/promise.h"
#include "shell/common/gin_helper/wrappable.h"
#include "shell/common/process_singleton/process_singleton.h"

namespace base {
class FilePath;
}

namespace lynxtron {
#if BUILDFLAG(IS_WIN)
enum class JumpListResult : int;
#endif
}  // namespace lynxtron

namespace lynxtron::api {

class App : public gin_helper::DeprecatedWrappable<App>,
            public gin_helper::EventEmitterMixin<App>,
            public lynxtron::ApplicationObserver {
 public:
  static gin_helper::Handle<App> Create(v8::Isolate* isolate);
  static App* Get();

  // gin::Wrappable
  static gin::DeprecatedWrapperInfo kWrapperInfo;
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;
  base::FilePath GetAppPath() const;
  static bool IsPackaged();

  App();

  // disable copy
  App(const App&) = delete;
  App& operator=(const App&) = delete;

 private:
  ~App() override;

  // ApplicationObserver:
  void OnBeforeQuit(bool* prevent_default) override;
  void OnWillQuit(bool* prevent_default) override;
  void OnWindowAllClosed() override;
  void OnQuit() override;
  void OnOpenFile(bool* prevent_default, const std::string& file_path) override;
  void OnOpenURL(const std::string& url) override;
  void OnActivate(bool has_visible_windows) override;
  void OnWillFinishLaunching() override;
  void OnFinishLaunching(base::Value::Dict launch_info) override;
  void OnPreMainMessageLoopRun() override;

#if BUILDFLAG(IS_MAC)
  void OnWillContinueUserActivity(bool* prevent_default,
                                  const std::string& type) override;
  void OnDidFailToContinueUserActivity(const std::string& type,
                                       const std::string& error) override;
  void OnContinueUserActivity(bool* prevent_default,
                              const std::string& type,
                              base::Value::Dict user_info,
                              base::Value::Dict details) override;
  void OnUserActivityWasContinued(const std::string& type,
                                  base::Value::Dict user_info) override;
  void OnUpdateUserActivityState(bool* prevent_default,
                                 const std::string& type,
                                 base::Value::Dict user_info) override;
  void OnNewWindowForTab() override;
  void OnDidBecomeActive() override;
  void OnDidResignActive() override;
#endif

  void SetAppPath(const base::FilePath& app_path);
  void SetAppLogsPath(gin_helper::ErrorThrower thrower,
                      std::optional<base::FilePath> custom_path);
  // Get/Set the pre-defined path in PathService.
  base::FilePath GetPath(gin_helper::ErrorThrower thrower,
                         const std::string& name);
  void SetPath(gin_helper::ErrorThrower thrower,
               const std::string& name,
               const base::FilePath& path);

  void SetDesktopName(const std::string& desktop_name);
  std::string GetLocale();
  std::string GetLocaleCountryCode();
  void OnSecondInstance(const base::CommandLine cmd,
                        const base::FilePath& cwd,
                        const std::vector<uint8_t> additional_data);
  bool HasSingleInstanceLock() const;
  bool RequestSingleInstanceLock(gin::Arguments* args);
  void ReleaseSingleInstanceLock();
  bool Relaunch(gin::Arguments* args);
  lynxtron::LoginItemSettings GetLoginItemSettings(gin::Arguments* args);
  v8::Local<v8::Promise> GetFileIcon(const base::FilePath& path,
                                     gin::Arguments* args);

  gin_helper::Dictionary GetAppMetrics(v8::Isolate* isolate);
  v8::Local<v8::Value> GetGPUFeatureStatus(v8::Isolate* isolate);
  v8::Local<v8::Promise> GetGPUInfo(v8::Isolate* isolate,
                                    const std::string& info_type);

#if BUILDFLAG(IS_MAC)
  void SetActivationPolicy(gin_helper::ErrorThrower thrower,
                           const std::string& policy);
  bool MoveToApplicationsFolder(gin_helper::ErrorThrower, gin::Arguments* args);
  bool IsInApplicationsFolder();
  v8::Local<v8::Value> GetDockAPI(v8::Isolate* isolate);
  // bool IsRunningUnderRosettaTranslation() const;
  v8::Global<v8::Value> dock_;
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  bool IsRunningUnderARM64Translation() const;
#endif

#if defined(MAS_BUILD)
  base::RepeatingCallback<void()> StartAccessingSecurityScopedResource(
      gin::Arguments* args);
#endif
  void OnRunAsElevatedComplete(gin_helper::Promise<bool> promise,
                               bool success,
                               int32_t code);

#if BUILDFLAG(IS_WIN)
  // Get the current Jump List settings.
  v8::Local<v8::Value> GetJumpListSettings();

  // Set or remove a custom Jump List for the application.
  JumpListResult SetJumpList(v8::Local<v8::Value> val, gin::Arguments* args);

  v8::Local<v8::Promise> closeStartupWindow();
  bool isStartupWindowShowing();
  std::string getMACAddress();
  std::string getBaseboardUUID();
  std::string getDiskSerialNumber();
  std::string getDiskSerialNumberLegacy();
  std::string getDeviceModalName();
  void RunElevatedInThread(const std::string& path,
                           const std::vector<std::string>& cmd_args,
                           bool fallback,
                           gin_helper::Promise<bool> promise);

  // starting speed
  int64_t GetLauncherProcessCreationTime();
  int64_t GetMainProcessCreationTime();
  int64_t GetLynxProcessCreationTime();

#endif  // BUILDFLAG(IS_WIN)

  v8::Local<v8::Promise> runAsElevated(gin::Arguments* args);
  std::string getChannelID();
  std::string getGuid();

  std::unique_ptr<ProcessSingleton> process_singleton_;
  bool watch_singleton_socket_on_ready_ = false;

  // Tracks tasks requesting file icons.
  base::CancelableTaskTracker cancelable_task_tracker_;
  base::FilePath app_path_;
  // TODO(Guo Xi):process metric
  // std::unique_ptr<electron::ProcessMetric> app_metric_;

  base::WeakPtrFactory<App> weak_factory_{this};
};

}  // namespace lynxtron::api

#endif  // LYNXTRON_SHELL_API_API_APP_H_
