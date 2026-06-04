// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_API_NATIVE_WINDOW_WIN_H_
#define LYNXTRON_SHELL_API_NATIVE_WINDOW_WIN_H_

#include <shobjidl.h>

#include <wrl/client.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/windows_types.h"
#include "shell/app/native_window.h"
#include "shell/ui/gfx/geometry/rect.h"
#include "shell/ui/platform_window/win/hwnd_message_handler.h"
#include "shell/ui/platform_window/win/hwnd_message_handler_delegate.h"

namespace lynxtron {

extern gfx::Rect ScreenToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds);

class NativeWindowWin : public NativeWindow,
                        public NativeWindowObserver,
                        public ui::HWNDMessageHandlerDelegate {
 public:
  NativeWindowWin(const gin_helper::Dictionary& options, NativeWindow* parent);
  NativeWindowWin(const NativeWindowWin&) = delete;
  NativeWindowWin& operator=(const NativeWindowWin&) = delete;
  ~NativeWindowWin() override;

  void Close() override;
  void CloseImmediately() override;
  void Focus(bool focus) override;
  bool IsFocused() override;
  void Show() override;
  void ShowInactive() override;
  void Hide() override;
  bool IsVisible() override;
  bool IsEnabled() override;
  void SetEnabled(bool enable) override;
  void Maximize() override;
  void Unmaximize() override;
  bool IsMaximized() const override;
  void Minimize() override;
  void Restore() override;
  bool IsMinimized() const override;
  void SetFullScreen(bool fullscreen) override;
  bool IsFullscreen() const override;
  void SetBounds(const gfx::Rect& bounds, bool animate) override;
  gfx::Rect GetBounds() const override;
  float GetDevicePixelRatio() const override;
  gfx::Rect GetNormalBounds() const override;
  void SetResizable(bool resizable) override;
  void MoveTop() override;
  bool IsResizable() const override;
  void SetAspectRatio(double aspect_ratio,
                      const gfx::Size& extra_size) override;
  void SetMovable(bool movable) override;
  bool IsMovable() const override;
  void SetMinimizable(bool minimizable) override;
  bool IsMinimizable() const override;
  void SetMaximizable(bool maximizable) override;
  bool IsMaximizable() const override;
  void SetFullScreenable(bool fullscreenable) override;
  bool IsFullScreenable() const override;
  void SetClosable(bool closable) override;
  bool IsClosable() const override;
  void SetAlwaysOnTop(ui::ZOrderLevel z_order,
                      const std::string& level,
                      int relativeLevel) override;
  ui::ZOrderLevel GetZOrderLevel() const override;
  void Center() override;
  void SetTitle(const std::string& title) override;
  std::string GetTitle() const override;
  void FlashFrame(bool flash) override;
  void SetSkipTaskbar(bool skip) override;
  void SetExcludedFromShownWindowsMenu(bool excluded) override {}
  bool IsExcludedFromShownWindowsMenu() override;
  void SetSimpleFullScreen(bool simple_fullscreen) override;
  bool IsSimpleFullScreen() override;
  void SetBackgroundColor(SkColor background_color) override;
  SkColor GetBackgroundColor() const override;
  void SetHasShadow(bool has_shadow) override;
  bool HasShadow() override;
  void SetOpacity(const double opacity) override;
  double GetOpacity() override;
  void SetFocusable(bool focusable) override;
  bool IsFocusable() const override;
  void SetParentWindow(NativeWindow* parent) override;
  void SetVisibleOnAllWorkspaces(bool visible,
                                 bool visibleOnFullScreen,
                                 bool skipTransformProcessType) override;

  bool IsVisibleOnAllWorkspaces() override;
  NativeWindowHandle GetNativeWindowHandle() const override;
  void IncrementChildModals() override;
  void DecrementChildModals() override;
  void SetIcon(api::NativeImage* icon) override;
  void SetProgressBar(double progress, const ProgressState state) override;
  void SetIcon(HICON small_icon, HICON app_icon);
  ui::WindowShowState GetRestoredState();

  // Overridden from HWNDMessageHandlerDelegate:
  ui::FrameMode GetFrameMode() const override;
  bool HasFrame() const override;
  bool ShouldPaintAsActive() const override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanActivate() const override;
  bool WantsMouseEventsWhenInactive() const override;
  bool IsModal() const override;
  int GetInitialShowState() const override;
  bool GetClientAreaInsets(gfx::Insets* insets,
                           HMONITOR monitor) const override;
  bool GetDwmFrameInsetsInPixels(gfx::Insets* insets) const override;
  void GetMinMaxSize(gfx::Size* min_size, gfx::Size* max_size) const override;
  gfx::Size GetRootViewSize() const override;
  gfx::Size DIPToScreenSize(const gfx::Size& dip_size) const override;
  void ResetWindowControls() override;
  void HandleActivationChanged(bool active) override;
  bool HandleAppCommand(int command) override;
  void HandleCancelMode() override;
  void HandleClose() override;
  bool HandleCommand(int command) override;
  void HandleCreate() override;
  void HandleDestroying() override;
  void HandleDestroyed() override;
  bool HandleInitialFocus(ui::WindowShowState show_state) override;
  void HandleDisplayChange() override;
  void HandleBeginWMSizeMove() override;
  void HandleEndWMSizeMove() override;
  void HandleMove() override;
  void HandleMoved() override;
  bool HandleMoving(RECT* rect) override;
  void HandleWorkAreaChanged() override;
  void HandleVisibilityChanged(bool visible) override;
  void HandleWindowMinimizedOrRestored(bool restored) override;
  void HandleWindowMaximized(bool maximized) override;
  void HandleClientSizeChanged(const gfx::Size& new_size) override;
  void HandleFrameChanged() override;
  void HandleNativeFocus(HWND last_focused_window) override;
  void HandleNativeBlur(HWND focused_window) override;
  void HandleMenuLoop(bool in_menu_loop) override;
  bool PreHandleMSG(UINT message,
                    WPARAM w_param,
                    LPARAM l_param,
                    LRESULT* result) override;
  void PostHandleMSG(UINT message, WPARAM w_param, LPARAM l_param) override;
  void HandleWindowSizeChanging() override;
  void HandleWindowSizeUnchanged() override;
  void HandleWindowScaleFactorChanged(float window_scale_factor) override;
  gfx::Rect ContentBoundsToWindowBounds(const gfx::Rect& bounds) const override;
  gfx::Rect WindowBoundsToContentBounds(const gfx::Rect& bounds) const override;

  void NotifyWindowMessage(UINT message, WPARAM w_param, LPARAM l_param);

 private:
  // Enable/disable:
  bool ShouldBeEnabled();
  void SetEnabledInternal(bool enabled);
  // Maintain window placement.
  void MoveBehindTaskBarIfNeeded();

  ui::WindowShowState last_window_state_;
  gfx::Rect last_normal_placement_bounds_;
  // Whether to show the WS_THICKFRAME style.
  bool thick_frame_ = true;
  // The bounds of window before maximize/fullscreen.
  gfx::Rect restore_bounds_;
  // The icons of window and taskbar.
  base::win::ScopedGDIObject<HICON> window_icon_;
  base::win::ScopedGDIObject<HICON> app_icon_;
  base::win::ScopedGDIObject<HBRUSH> background_brush_;
  bool layered_ = false;
  // Set to true if the window is always on top and behind the task bar.
  bool behind_task_bar_ = false;
  // Whether the window is currently being moved.
  bool is_moving_ = false;
  // Whether the window should be enabled based on user calls to SetEnabled()
  bool is_enabled_ = true;
  // How many modal children this window has;
  // used to determine enabled state
  unsigned int num_modal_children_ = 0;
  bool movable_ = true;
  bool fullscreenable_ = true;
  std::string title_;
  double opacity_ = 1.0;
  bool can_activate_ = true;
  SizeConstraints old_size_constraints_;
  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;
  Microsoft::WRL::ComPtr<ITaskbarList3> taskbar_list_;
  std::unique_ptr<ui::HWNDMessageHandler> window_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_NATIVE_WINDOW_WIN_H_
