// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_API_LYNX_WINDOW_H_
#define LYNXTRON_SHELL_API_API_LYNX_WINDOW_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "shell/api/api_base_window.h"
#include "shell/api/lynx_view/headless_windowless_renderer.h"
#include "shell/api/lynx_view/lynx_view.h"
#include "shell/api/lynx_view/lynx_view_client.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/error_thrower.h"

namespace lynxtron {
class LynxViewStateObserver;
}

namespace lynxtron::api {
class LynxWindow : public BaseWindow, public lynxtron::LynxViewClient {
 public:
  static gin_helper::WrappableBase* New(gin_helper::ErrorThrower thrower,
                                        gin::Arguments* args);

  static void BuildPrototype(v8::Isolate* isolate,
                             v8::Local<v8::FunctionTemplate> prototype);

  static v8::Local<v8::Value> From(v8::Isolate* isolate,
                                   NativeWindow* native_window);

  base::WeakPtr<LynxWindow> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }
  std::string ResolveResourceUrl(const std::string& resource_url) const;

  bool OpenScheme(const std::string& url, gin::Arguments* args);
  void OpenOnlineScheme(const std::string& pc_open_type,
                        const std::string& escaped_url,
                        const std::string& global_props,
                        const std::string& initial_props,
                        const std::string& group_name,
                        const std::string& channel_name,
                        const std::string& scheme);

  bool SendGlobalEventSafely(const LynxView* lynx_view,
                             const std::string& name,
                             const std::string& json_params);

  void ReportJSError(const std::string& error_info);
  void ConfigJSBase(const std::string& js_base);
  void CustomReport(const std::string& custom_data);

  void SetFpsMonitorEnabled(bool enabled, uint32_t sample_interval_millis);

 protected:
  LynxWindow(gin::Arguments* args, const gin_helper::Dictionary& options);

  LynxWindow(const LynxWindow&) = delete;
  LynxWindow& operator=(const LynxWindow&) = delete;
  ~LynxWindow() override;

  void CloseImmediately() override;
  void Focus() override;
  void Blur() override;

  void OnWindowClosed() override;
  void OnWindowBlur() override;
  void OnWindowFocus() override;
  void OnWindowIsKeyChanged(bool is_key) override;
  void OnWindowShow() override;
  void OnWindowHide() override;
  void OnWindowMinimize() override;
  void OnWindowRestore() override;

  void OnWindowResize() override;
  void OnWindowResized() override;
  void OnWindowWillMove(const gfx::Rect& new_bounds,
                        bool& prevent_default) override;
  void OnWindowMove() override;
  void OnWindowLeaveFullScreen() override;
#if defined(OS_WIN)
  void OnWindowMessage(UINT message, WPARAM w_param, LPARAM l_param) override;
#endif

  bool RequestLayoutWhenSafepointEnable();
  bool LoadUrl(const std::string& url, gin::Arguments* args);
  bool LoadFile(const std::string& path, gin::Arguments* args);
  bool LoadBundle(gin::Arguments* args);
  bool UpdateMetaData(gin::Arguments* args);
  bool UpdateData(const gin_helper::Dictionary& data,
                  const gin_helper::Dictionary& global_props);
  bool SetGlobalProps(const gin_helper::Dictionary& global_props);
  bool SendGlobalEvent(const std::string& name,
                       const gin_helper::Dictionary& json);
  v8::Local<v8::Value> CaptureHeadlessFrame();
  std::string DumpHeadlessUITreeForCDP();
  v8::Local<v8::Value> GetHeadlessMetrics();
  bool DispatchHeadlessPointerEvent(gin::Arguments* args);
  bool PumpHeadlessTasks();
  bool ReloadTemplate(const gin_helper::Dictionary& data,
                      const gin_helper::Dictionary& global_props);

  bool CheckLynxViewExit(const LynxView* lynx_view);
  void ReportErrorToNode(const std::string& error_type,
                         const int32_t error_code,
                         const std::string& message);

 private:
  bool ComputeRenderActive() const;
  void SyncRenderActiveState();
  void EnsureLynxView();
  void OnPageStart(std::string_view url) override;
  void OnLoadSuccess() override;
  void OnFirstScreen() override;
  void OnPageUpdated() override;
  void OnDataUpdated() override;
  void OnDestroy() override;
  void OnRuntimeReady() override;
  void OnReceivedError(int error_code, std::string_view message) override;
  void OnTimingSetup(std::string_view timing_info) override;
  void OnTimingUpdate(std::string_view timing_info,
                      std::string_view update_timing,
                      std::string_view update_flag) override;
  void OnEnterForeground() override;
  void OnEnterBackground() override;
  void OnFrameTiming(int64_t frame_start_time_in_ns,
                     int64_t frame_finish_time_in_ns) override;

  void ScheduleUnresponsiveEvent(int ms);
  void NotifyWindowUnresponsive();

  void Send(const std::string& channel, gin::Arguments* arguments);

  void LynxViewInit(const std::string& key,
                    const std::string& global_props = "",
                    const std::string& group_name = "");

  void OpenLynxWithType(const std::string& pc_open_type,
                        const std::string& key,
                        const std::string& local_url,
                        const std::string& global_props = "",
                        const std::string& initial_props = "",
                        const std::string& group_name = "",
                        const std::string& channel_name = "",
                        const std::string& scheme = "");

  void StartFpsMonitorTask();
  void EmitFpsEvent();
  void SetTemplateResourceBaseFromFile(const base::FilePath& path);
  void SetTemplateResourceBaseFromUrl(const std::string& url);

#if defined(OS_WIN)
  friend class LynxNativeModule;
  friend class HybridMonitorModule;
  std::shared_ptr<LynxNativeModule> lynx_bridge_;
  std::shared_ptr<HybridMonitorModule> hybrid_monitor_;
#endif

  bool software_render_ = true;
  bool headless_ = false;
  std::optional<float> headless_device_pixel_ratio_ = std::nullopt;
  std::shared_ptr<HeadlessWindowlessRenderer> headless_renderer_;
  std::vector<std::string> node_integration_preload_ = {};
  std::unique_ptr<LynxView> lynx_view_;
  static bool lynx_global_init_;
  bool enable_fps_monitor_ = false;
  int sample_interval_millis_ = 1000;
  std::vector<std::vector<int64_t>> last_frame_timings_;
  uint64_t headless_task_pumps_ = 0;
  bool last_render_active_ = false;
  base::WeakPtrFactory<LynxWindow> weak_factory_{this};
  std::unique_ptr<LynxViewStateObserver> lynx_view_state_observer_;
  std::optional<std::string> data_str_ = std::nullopt;
  std::optional<std::string> global_props_ = std::nullopt;
  std::optional<std::string> current_resource_base_url_ = std::nullopt;
  bool current_resource_base_is_file_ = false;
};

}  // namespace lynxtron::api

#endif  // LYNXTRON_SHELL_API_API_LYNX_WINDOW_H_
