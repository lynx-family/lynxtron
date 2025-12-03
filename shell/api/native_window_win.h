// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef SHELL_BROWSER_LYNX_NATIVE_WINDOW_VIEWS_H_
#define SHELL_BROWSER_LYNX_NATIVE_WINDOW_VIEWS_H_

#include <memory>
#include <set>
#include <string>
#include <vector>
// #include "electron/buildflags/buildflags.h"

#include "base/win/windows_types.h"
#include "shell/app/native_window.h"
#include "shell/ui/gfx/geometry/rect.h"
#include "shell/ui/platform_window/win/hwnd_message_handler.h"
#include "shell/ui/platform_window/win/hwnd_message_handler_delegate.h"
// using ui::FrameMode;
using ui::HWNDMessageHandler;
using ui::HWNDMessageHandlerDelegate;

#if BUILDFLAG(IS_WIN)
#include "base/memory/raw_ptr.h"
#include "base/win/scoped_gdi_object.h"
#endif

namespace lynxtron {
class WindowStateWatcher;

#if BUILDFLAG(IS_WIN)
extern gfx::Rect ScreenToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds);
#endif

class NativeWindowWin : public NativeWindow,
                        public NativeWindowObserver,
                        public ui::HWNDMessageHandlerDelegate {
 public:
  NativeWindowWin(const gin_helper::Dictionary& options, NativeWindow* parent);
  NativeWindowWin(const NativeWindowWin&) = delete;
  NativeWindowWin& operator=(const NativeWindowWin&) = delete;
  ~NativeWindowWin() override;

  // TODO(jiangwenlong) : support These functions
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
  gfx::Rect GetNormalBounds() const override;
  void SetContentSizeConstraints(
      const SizeConstraints& size_constraints) override;
  SizeConstraints GetContentSizeConstraints() const override;
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
  void SetKiosk(bool kiosk) override;
  bool IsKiosk() const override;
  bool IsTabletMode() const override;
  // void SetBackgroundColor(SkColor color) override;
  void SetHasShadow(bool has_shadow) override;
  bool HasShadow() override;
  void SetOpacity(const double opacity) override;
  double GetOpacity() override;
  // void SetIgnoreMouseEvents(bool ignore, bool forward) override;
  void SetContentProtection(bool enable) override;
  void SetFocusable(bool focusable) override;
  bool IsFocusable() const override;
  void SetParentWindow(NativeWindow* parent) override;
  void SetVisibleOnAllWorkspaces(bool visible,
                                 bool visibleOnFullScreen,
                                 bool skipTransformProcessType) override;

  bool IsVisibleOnAllWorkspaces() override;

  // void SetGTKDarkThemeEnabled(bool use_dark_theme) override;

  NativeWindowHandle GetNativeWindowHandle() const override;

  void IncrementChildModals() override;
  void DecrementChildModals() override;

#if BUILDFLAG(IS_WIN)
  // Catch-all message handling and filtering. Called before
  // HWNDMessageHandler's built-in handling, which may pre-empt some
  // expectations in Views/Aura if messages are consumed. Returns true if the
  // message was consumed by the delegate and should not be processed further
  // by the HWNDMessageHandler. In this case, |result| is returned. |result| is
  // not modified otherwise.
  // bool PreHandleMSG(UINT message,
  //                  WPARAM w_param,
  //                  LPARAM l_param,
  //                  LRESULT* result);
  void SetIcon(HICON small_icon, HICON app_icon);

  bool IsWindowControlsOverlayEnabled() const {
    return (title_bar_style_ == NativeWindowWin::TitleBarStyle::kHidden) &&
           titlebar_overlay_;
  }
#endif

  // Returns the restore state for the window.
  ui::WindowShowState GetRestoredState();

  // Overridden from HWNDMessageHandlerDelegate:
  bool HasNonClientView() const override;
  ui::FrameMode GetFrameMode() const override;
  bool HasFrame() const override;
  void SchedulePaint() override;
  bool ShouldPaintAsActive() const override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanActivate() const override;
  bool WantsMouseEventsWhenInactive() const override;
  bool IsModal() const override;
  int GetInitialShowState() const override;
  // int GetNonClientComponent(const gfx::Point& point) const override;
  // void GetWindowMask(const gfx::Size& size, SkPath* path) override;
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
  void HandleCaptureLost() override;
  void HandleClose() override;
  bool HandleCommand(int command) override;
  // void HandleAccelerator(const ui::Accelerator& accelerator) override;
  void HandleCreate() override;
  void HandleDestroying() override;
  void HandleDestroyed() override;
  bool HandleInitialFocus(ui::WindowShowState show_state) override;
  void HandleDisplayChange() override;
  void HandleBeginWMSizeMove() override;
  void HandleEndWMSizeMove() override;
  void HandleMove() override;
  bool HandleMoving(RECT* rect) override;
  void HandleWorkAreaChanged() override;
  void HandleVisibilityChanged(bool visible) override;
  void HandleWindowMinimizedOrRestored(bool restored) override;
  void HandleClientSizeChanged(const gfx::Size& new_size) override;
  void HandleFrameChanged() override;
  void HandleNativeFocus(HWND last_focused_window) override;
  void HandleNativeBlur(HWND focused_window) override;
  // bool HandleMouseEvent(ui::MouseEvent* event) override;
  // void HandleKeyEvent(ui::KeyEvent* event) override;
  // void HandleTouchEvent(ui::TouchEvent* event) override;
  // bool HandleScrollEvent(ui::ScrollEvent* event) override;
  // bool HandleGestureEvent(ui::GestureEvent* event) override;
  // bool HandleMouseEventForCaption(UINT message) const override;

  void HandleInputLanguageChange(DWORD character_set,
                                 HKL input_language_id) override;
  bool HandleTooltipNotify(int w_param,
                           NMHDR* l_param,
                           LRESULT* l_result) override;
  void HandleMenuLoop(bool in_menu_loop) override;
  bool PreHandleMSG(UINT message,
                    WPARAM w_param,
                    LPARAM l_param,
                    LRESULT* result) override;
  void PostHandleMSG(UINT message, WPARAM w_param, LPARAM l_param) override;

  void HandleWindowSizeChanging() override;
  void HandleWindowSizeUnchanged() override;
  void HandleWindowScaleFactorChanged(float window_scale_factor) override;

  // gfx::AcceleratedWidget GetAcceleratedWidget() const override;

  gfx::Rect ContentBoundsToWindowBounds(const gfx::Rect& bounds) const override;
  gfx::Rect WindowBoundsToContentBounds(const gfx::Rect& bounds) const override;

 private:
  void HandleSizeEvent(WPARAM w_param, LPARAM l_param);
  // void SetForwardMouseMessages(bool forward);
  static LRESULT CALLBACK SubclassProc(HWND hwnd,
                                       UINT msg,
                                       WPARAM w_param,
                                       LPARAM l_param,
                                       UINT_PTR subclass_id,
                                       DWORD_PTR ref_data);
  static LRESULT CALLBACK MouseHookProc(int n_code,
                                        WPARAM w_param,
                                        LPARAM l_param);

  // Enable/disable:
  bool ShouldBeEnabled();
  void SetEnabledInternal(bool enabled);

  // Maintain window placement.
  void MoveBehindTaskBarIfNeeded();

#if defined(OS_WIN)

  ui::WindowShowState last_window_state_;

  gfx::Rect last_normal_placement_bounds_;

  // In charge of running taskbar related APIs.
  // TaskbarHost taskbar_host_;

  // Memoized version of a11y check
  bool checked_for_a11y_support_ = false;

  // Whether to show the WS_THICKFRAME style.
  bool thick_frame_ = true;

  // The bounds of window before maximize/fullscreen.
  gfx::Rect restore_bounds_;

  // The icons of window and taskbar.
  base::win::ScopedGDIObject<HICON> window_icon_;
  base::win::ScopedGDIObject<HICON> app_icon_;

  // The set of windows currently forwarding mouse messages.
  // static std::set<NativeWindowWin*> forwarding_windows_;
  static HHOOK mouse_hook_;
  HWND legacy_window_ = nullptr;
  bool layered_ = false;

  // Set to true if the window is always on top and behind the task bar.
  bool behind_task_bar_ = false;

  // Whether we want to set window placement without side effect.
  // bool is_setting_window_placement_ = false;

  // Whether the window is currently being resized.
  bool is_resizing_ = false;

  // Whether the window is currently being moved.
  bool is_moving_ = false;
#endif

  // Whether the window should be enabled based on user calls to SetEnabled()
  bool is_enabled_ = true;
  // How many modal children this window has;
  // used to determine enabled state
  unsigned int num_modal_children_ = 0;

  bool use_content_size_ = false;
  bool movable_ = true;
  bool resizable_ = true;
  bool maximizable_ = true;
  bool minimizable_ = true;
  bool fullscreenable_ = true;
  bool activatable_ = true;
  std::string title_;
  gfx::Size widget_size_;
  // gfx::Size new_widget_size_;
  double opacity_ = 1.0;
  bool widget_destroyed_ = false;
  bool follow_parent_ = false;
  gfx::Vector2d parent_offset_;
  bool can_activate_ = true;
  std::optional<gfx::Rect> pending_bounds_change_;
  // The "resizable" flag on Linux is implemented by setting size constraints,
  // we need to make sure size constraints are restored when window becomes
  // resizable again. This is also used on Windows, to keep taskbar resize
  // events from resizing the window.
  SizeConstraints old_size_constraints_;
  std::unique_ptr<HWNDMessageHandler> window_;

  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;
};

}  // namespace lynxtron

#endif  // SHELL_BROWSER_LYNX_NATIVE_WINDOW_VIEWS_H_
