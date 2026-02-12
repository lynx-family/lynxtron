// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_API_API_LYNX_WINDOW_H_
#define LYNXTRON_SHELL_API_API_LYNX_WINDOW_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/cancelable_callback.h"
#include "common/gin_helper/dictionary.h"
#include "shell/api/api_base_window.h"
#include "shell/api/lynx_view/lynx_view.h"
#include "shell/api/lynx_view/lynx_view_client.h"
#include "shell/common/gin_helper/error_thrower.h"

namespace lynxtron::api {
std::map<std::string, std::string> GetQueryKeyValueMap(const std::string& spec);

class LynxWindow : public BaseWindow, public lynxtron::LynxViewClient {
 public:
  static gin_helper::WrappableBase* New(gin_helper::ErrorThrower thrower,
                                        gin::Arguments* args);

  static void BuildPrototype(v8::Isolate* isolate,
                             v8::Local<v8::FunctionTemplate> prototype);

  // Returns the BrowserWindow object from |native_window|.
  static v8::Local<v8::Value> From(v8::Isolate* isolate,
                                   NativeWindow* native_window);

  base::WeakPtr<LynxWindow> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

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

  // bool CloseByLynxBridge(const LynxView* lynx_view,
  //                        const std::string& animation_type,
  //                        const lynx::EncodableList& container_list);

  bool ReportJSError(const LynxView* lynx_view, const std::string& error_info);
  bool ConfigJSBase(const LynxView* lynx_view, const std::string& bid);
  bool CustomReport(const LynxView* lynx_view, const std::string& custom_data);

 protected:
  LynxWindow(gin::Arguments* args, const gin_helper::Dictionary& options);

  LynxWindow(const LynxWindow&) = delete;
  LynxWindow& operator=(const LynxWindow&) = delete;
  ~LynxWindow() override;

  // NativeWindowObserver:
  void RequestPreferredWidth(int* width) override;
  void OnCloseButtonClicked(bool& prevent_default) override;
  void OnWindowIsKeyChanged(bool is_key) override;
  void OnWindowClosed() override;
#if defined(OS_WIN)
  void OnWindowMessage(UINT message, WPARAM w_param, LPARAM l_param) override;
#endif

  // BaseWindow:
  void OnWindowBlur() override;
  void OnWindowFocus() override;
  void OnWindowResize() override;
  void OnWindowResized() override;
  void OnWindowRestore() override;
  void OnWindowMinimize() override;
  void OnWindowMove() override;
  void OnWindowWillMove(const gfx::Rect& new_bounds,
                        bool& prevent_default) override;
  void OnWindowLeaveFullScreen() override;
  void CloseImmediately() override;
  void Focus() override;
  void Blur() override;
  void SetBackgroundColor(const std::string& color_name) override;
  void OnWindowShow() override;
  void OnWindowHide() override;

  // Lynx
  bool RequestLayoutWhenSafepointEnable();
  bool UpdateDataWithString(const std::string& data);
  bool LoadFile(const std::string& url, gin::Arguments* args);
  bool SendGlobalEvent(const std::string& name,
                       const gin_helper::Dictionary& json);
  // bool SetGlobalProp(v8::Isolate* isolate, v8::Local<v8::Value> value);
  bool UpdateData(const gin_helper::Dictionary& data,
                  const gin_helper::Dictionary& global_props);
  bool ReloadTemplate(const gin_helper::Dictionary& data,
                      const gin_helper::Dictionary& global_props);

  bool CheckLynxViewExit(const LynxView* lynx_view);
  // LynxViewHolder* CurrentLynxViewHolder();
  void ReportErrorToNode(const std::string& error_type,
                         const int32_t error_code,
                         const std::string& message);
  // bool SendGlobalEvent(const std::string& event, const std::string& json);

 private:
  // impl of LynxViewHolderGroupClient
  // void OnPageStart(LynxView* lynx_view_holder,
  //                  const std::string& url) override;

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

  // void onLoadSuccess(LynxView* lynx_view_holder) override;

  // void onFirstScreen(LynxView* lynx_view_holder) override;

  // void OnDestroy(LynxView* lynx_view_holder) override;

  // void OnRuntimeReady(LynxView* lynx_view_holder) override;

  // void onErrorOccurred(LynxView* lynx_view_holder,
  //                      int32_t error_code,
  //                      std::string_view message) override;

  // void OnReceivedError(int error_code, std::string_view message) override;

  // void OnFirstLoadPerfReady(
  //     LynxView* lynx_view_holder,
  //     const std::unordered_map<int32_t, double>& perf,
  //     const std::unordered_map<int32_t, std::string>& perf_timing) override;

  // Schedule a notification unresponsive event.
  void ScheduleUnresponsiveEvent(int ms);

  // Dispatch unresponsive event to observers.
  void NotifyWindowUnresponsive();

  void Send(const std::string& channel, gin::Arguments* arguments);

  void CreateLynxView(const std::string& local_url,
                      const std::string& global_props = "",
                      const std::string& initial_props = "",
                      const std::string& group_name = "",
                      const std::string& channel_name = "",
                      const std::string& scheme = "");

  void LynxViewInit(const std::string& key,
                    const std::string& global_props = "",
                    const std::string& group_name = "");

  void LoadTemplate(const std::string& key,
                    const std::string& initial_props,
                    const std::string& channel_name,
                    const std::string& scheme);

  void OpenLynxWithType(const std::string& pc_open_type,
                        const std::string& key,
                        const std::string& local_url,
                        const std::string& global_props = "",
                        const std::string& initial_props = "",
                        const std::string& group_name = "",
                        const std::string& channel_name = "",
                        const std::string& scheme = "");
  void CloseLynxView();
  void FocusLynxView();

  // void CheckLynxValidation(const std::vector<uint8_t>& source,
  //                          const std::string& channel_name,
  //                          const std::string& scheme,
  //                          base::RepeatingCallback<void(bool)> callback);
  // void LynxVerify(const std::vector<uint8_t>& source,
  //                 const std::string& scheme,
  //                 int lynx_verify_mode,
  //                 const std::string& public_key,
  //                 base::RepeatingCallback<void(bool)> callback);

  // Closure that would be called when window is unresponsive when closing,
  // it should be cancelled when we can prove that the window is responsive.
  // base::CancelableRepeatingClosure window_unresponsive_closure_;

  // std::vector<mojom::DraggableRegionPtr> draggable_regions_;

  // std::unique_ptr<lynx::LynxViewHolderGroup> lynx_view_holder_group_ =
  //     std::make_unique<lynx::LynxViewHolderGroup>();

#if defined(OS_WIN)
  friend class LynxNativeModule;
  friend class HybridMonitorModule;
  std::shared_ptr<LynxNativeModule> lynx_bridge_;
  std::shared_ptr<HybridMonitorModule> hybrid_monitor_;
#endif
  // std::unique_ptr<lynx::LynxResourceProvider> js_resource_provider_;
  // std::unique_ptr<lynx::LynxResourceProvider> dynamic_component_provider_;

  // std::unordered_map<int, std::shared_ptr<httpclient::HttpClient>>
  //     http_clients_;

  // std::shared_ptr<LynxMonitor> lynx_monitor_ =
  // std::make_shared<LynxMonitor>();

  // lynx::EncodableMap global_props_;
  bool software_render_ = true;

  // node integration
  bool node_integration_ = false;

  std::unique_ptr<LynxView> lynx_view_;
  static bool lynx_global_init_;
  base::WeakPtrFactory<LynxWindow> weak_factory_{this};
};

}  // namespace lynxtron::api

#endif  // LYNXTRON_SHELL_API_API_LYNX_WINDOW_H_
