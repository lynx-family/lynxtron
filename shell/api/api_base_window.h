// Copyright (c) 2018 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_API_API_BASE_WINDOW_H_
#define LYNXTRON_SHELL_API_API_BASE_WINDOW_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "shell/app/native_window.h"
#include "shell/app/native_window_observer.h"
#include "shell/common/gin_helper/trackable_object.h"
#include "shell/common/global_thread.h"

namespace lynxtron::api {
class BaseWindow : public gin_helper::TrackableObject<BaseWindow>,
                   private NativeWindowObserver {
 public:
  static gin_helper::WrappableBase* New(gin_helper::Arguments* args);

  static void BuildPrototype(v8::Isolate* isolate,
                             v8::Local<v8::FunctionTemplate> prototype);

  const NativeWindow* window() const { return window_.get(); }
  NativeWindow* window() { return window_.get(); }

 protected:
  // Common constructor.
  BaseWindow(v8::Isolate* isolate, const gin_helper::Dictionary& options);
  // Creating independent BaseWindow instance.
  BaseWindow(gin_helper::Arguments* args,
             const gin_helper::Dictionary& options);
  ~BaseWindow() override;

  // TrackableObject:
  void InitWith(v8::Isolate* isolate, v8::Local<v8::Object> wrapper) override;

  // NativeWindowObserver:
  void WillCloseWindow(bool& prevent_default) override;
  void OnWindowClosed() override;
  void OnWindowEndSession() override;
  void OnWindowBlur() override;
  void OnWindowFocus() override;
  void OnWindowShow() override;
  void OnWindowHide() override;
  void OnWindowMaximize() override;
  void OnWindowUnmaximize() override;
  void OnWindowMinimize() override;
  void OnWindowRestore() override;
  void OnWindowWillResize(const gfx::Rect& new_bounds,
                          gfx::ResizeEdge edge,
                          bool& prevent_default) override;
  void OnWindowResize() override;
  void OnWindowResized() override;
  void OnWindowWillMove(const gfx::Rect& new_bounds,
                        bool& prevent_default) override;
  void OnWindowMove() override;
  void OnWindowMoved() override;
  void OnWindowScrollTouchBegin() override;
  void OnWindowScrollTouchEnd() override;
  void OnWindowSwipe(const std::string& direction) override;
  void OnWindowRotateGesture(float rotation) override;
  void OnWindowSheetBegin() override;
  void OnWindowSheetEnd() override;
  void OnWindowEnterFullScreen() override;
  void OnWindowLeaveFullScreen() override;
  void OnWindowAlwaysOnTopChanged() override;
  void OnExecuteAppCommand(std::string_view command_name) override;
  void OnTouchBarItemResult(const std::string& item_id,
                            const base::Value::Dict& details) override;
  void OnSystemContextMenu(int x, int y, bool& prevent_default) override;
#if BUILDFLAG(IS_WIN)
  void OnWindowMessage(UINT message, WPARAM w_param, LPARAM l_param) override;
#endif

  // Public APIs of NativeWindow.
  void Close();
  virtual void CloseImmediately();
  virtual void Focus();
  virtual void Blur();
  bool IsFocused() const;
  void Show();
  void ShowInactive();
  void Hide();
  bool IsVisible() const;
  bool IsEnabled() const;
  void SetEnabled(bool enable);
  void Maximize();
  void Unmaximize();
  bool IsMaximized() const;
  void Minimize();
  void Restore();
  bool IsMinimized() const;
  void SetFullScreen(bool fullscreen);
  bool IsFullscreen() const;
  void SetBounds(const gfx::Rect& bounds, gin_helper::Arguments* args);
  gfx::Rect GetBounds() const;
  void SetSize(int width, int height, gin_helper::Arguments* args);
  std::array<int, 2U> GetSize() const;
  // void SetContentSize(int width, int height, gin_helper::Arguments* args);
  std::array<int, 2U> GetContentSize() const;
  // void SetContentBounds(const gfx::Rect& bounds, gin_helper::Arguments*
  // args);
  // gfx::Rect GetContentBounds() const;
  bool IsNormal() const;
  gfx::Rect GetNormalBounds() const;
  void SetMinimumSize(int width, int height);
  std::array<int, 2U> GetMinimumSize() const;
  void SetMaximumSize(int width, int height);
  std::array<int, 2U> GetMaximumSize() const;
  void SetSheetOffset(double offsetY, gin_helper::Arguments* args);
  void SetResizable(bool resizable);
  bool IsResizable() const;
  void SetMovable(bool movable);
  void MoveTop();
  bool IsMovable() const;
  void SetMinimizable(bool minimizable);
  bool IsMinimizable() const;
  void SetMaximizable(bool maximizable);
  bool IsMaximizable() const;
  void SetFullScreenable(bool fullscreenable);
  bool IsFullScreenable() const;
  void SetClosable(bool closable);
  bool IsClosable() const;
  void SetAlwaysOnTop(bool top, gin_helper::Arguments* args);
  bool IsAlwaysOnTop() const;
  void Center();
  void SetPosition(int x, int y, gin_helper::Arguments* args);
  std::array<int, 2U> GetPosition() const;
  void SetTitle(const std::string& title);
  std::string GetTitle() const;
  void SetAccessibleTitle(const std::string& title);
  std::string GetAccessibleTitle() const;
  void FlashFrame(bool flash);
  void SetSkipTaskbar(bool skip);
  void SetExcludedFromShownWindowsMenu(bool excluded);
  bool IsExcludedFromShownWindowsMenu() const;
  void SetSimpleFullScreen(bool simple_fullscreen);
  bool IsSimpleFullScreen() const;
  void SetKiosk(bool kiosk);
  bool IsKiosk() const;
  bool IsTabletMode() const;
  virtual void SetBackgroundColor(const std::string& color_name);
  std::string GetBackgroundColor() const;
  void InvalidateShadow();
  void SetHasShadow(bool has_shadow);
  bool HasShadow() const;
  void SetOpacity(const double opacity);
  double GetOpacity() const;
  void SetShape(const std::vector<gfx::Rect>& rects);
  void SetRepresentedFilename(const std::string& filename);
  std::string GetRepresentedFilename() const;
  void SetDocumentEdited(bool edited);
  bool IsDocumentEdited() const;
  // void SetIgnoreMouseEvents(bool ignore, gin_helper::Arguments* args);
  void SetContentProtection(bool enable);
  void SetFocusable(bool focusable);
  bool IsFocusable() const;
  void SetMenu(v8::Isolate* isolate, v8::Local<v8::Value> menu);
  void RemoveMenu();
  void SetParentWindow(v8::Local<v8::Value> value, gin_helper::Arguments* args);

  v8::Local<v8::Value> GetNativeWindowHandle();
  void SetVisibleOnAllWorkspaces(bool visible, gin_helper::Arguments* args);
  bool IsVisibleOnAllWorkspaces() const;
  void SetAutoHideCursor(bool auto_hide);
  virtual void SetVibrancy(v8::Isolate* isolate,
                           v8::Local<v8::Value> value,
                           gin_helper::Arguments* args);

#if BUILDFLAG(IS_MAC)
  std::string GetAlwaysOnTopLevel() const;
  void SetWindowButtonVisibility(bool visible);
  bool GetWindowButtonVisibility() const;
  void SetTrafficLightPosition(const gfx::Point& position);
  gfx::Point GetTrafficLightPosition() const;
#endif

  void SetTouchBar(std::vector<gin_helper::PersistentDictionary> items);
  void RefreshTouchBarItem(const std::string& item_id);
  void SetEscapeTouchBarItem(gin_helper::PersistentDictionary item);
  // void SelectPreviousTab();
  // void SelectNextTab();
  // void MergeAllWindows();
  // void MoveTabToNewWindow();
  // void ToggleTabBar();
  void SetAspectRatio(double aspect_ratio, gin_helper::Arguments* args);
  // void PreviewFile(const std::string& path, gin_helper::Arguments* args);
  // void CloseFilePreview();
  // void SetGTKDarkThemeEnabled(bool use_dark_theme);

  // Public getters of NativeWindow.
  v8::Local<v8::Value> GetParentWindow() const;
  v8::Local<v8::Value> GetBrowserParentWindow() const;
  std::vector<v8::Local<v8::Object>> GetChildWindows() const;
  bool IsModal() const;

  // Extra APIs added in JS.
  bool SetThumbarButtons(gin_helper::Arguments* args);

#if BUILDFLAG(IS_WIN)
  typedef base::RepeatingCallback<void(v8::Local<v8::Value>,
                                       v8::Local<v8::Value>)>
      MessageCallback;
  bool HookWindowMessage(UINT message, const MessageCallback& callback);
  bool IsWindowMessageHooked(UINT message);
  void UnhookWindowMessage(UINT message);
  void UnhookAllWindowMessages();
  bool SetThumbnailClip(const gfx::Rect& region);
  bool SetThumbnailToolTip(const std::string& tooltip);
  void SetAppDetails(const gin_helper::Dictionary& options);
#endif
  int32_t GetID() const;

  // Remove this window from parent window's |child_windows_|.
  void RemoveFromParentChildWindows();

  template <typename... Args>
  void EmitEventSoon(std::string_view eventName) {
    GetUIThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&BaseWindow::Emit<Args...>),
                       weak_factory_.GetWeakPtr(), eventName));
  }

  template <typename... Args>
  void EmitEventSoon(std::string_view eventName, Args&&... args) {
    GetUIThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&BaseWindow::Emit<Args...>),
                       weak_factory_.GetWeakPtr(), eventName,
                       std::forward<Args>(args)...));
  }

#if BUILDFLAG(IS_WIN)
  typedef std::map<UINT, MessageCallback> MessageCallbackMap;
  MessageCallbackMap messages_callback_map_;
#endif
  v8::Global<v8::Value> parent_window_;
  KeyWeakMap<int> child_windows_;
  std::unique_ptr<NativeWindow> window_;
  // Reference to JS wrapper to prevent garbage collection.
  v8::Global<v8::Value> self_ref_;

  base::WeakPtrFactory<BaseWindow> weak_factory_{this};
};

}  // namespace lynxtron::api

// namespace electron

#endif  // LYNXTRON_SHELL_API_API_BASE_WINDOW_H_
