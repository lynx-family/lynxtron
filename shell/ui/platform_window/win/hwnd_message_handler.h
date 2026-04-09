// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_PLATFORM_WINDOW_WIN_HWND_MESSAGE_HANDLER_H_
#define LYNXTRON_SHELL_UI_PLATFORM_WINDOW_WIN_HWND_MESSAGE_HANDLER_H_

#include <windows.h>

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "shell/ui/base/ui_base_types.h"
#include "shell/ui/gfx/geometry/rect.h"
#include "shell/ui/gfx/win/msg_util.h"
#include "shell/ui/gfx/win/window_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gfx {
class Insets;
}  // namespace gfx

namespace ui {

class FullscreenHandler;
class HWNDMessageHandlerDelegate;

// These two messages aren't defined in winuser.h, but they are sent to windows
// with captions. They appear to paint the window caption and frame.
// Unfortunately if you override the standard non-client rendering as we do
// with CustomFrameWindow, sometimes Windows (not deterministically
// reproducibly but definitely frequently) will send these messages to the
// window and paint the standard caption/title over the top of the custom one.
// So we need to handle these messages in CustomFrameWindow to prevent this
// from happening.
constexpr int WM_NCUAHDRAWCAPTION = 0xAE;
constexpr int WM_NCUAHDRAWFRAME = 0xAF;

// The HWNDMessageHandler sends this message to itself on
// WM_WINDOWPOSCHANGING. It's used to inform the client if a
// WM_WINDOWPOSCHANGED won't be received.
constexpr int WM_WINDOWSIZINGFINISHED = WM_USER;

// An object that handles messages for a HWND that implements the views
// "Custom Frame" look. The purpose of this class is to isolate the windows-
// specific message handling from the code that wraps it. It is intended to be
// used by both a views::NativeWidget and an aura::WindowTreeHost
// implementation.
// TODO(beng): This object should eventually *become* the WindowImpl.
class HWNDMessageHandler : public gfx::WindowImpl {
 public:
  // See WindowImpl for details on |debugging_id|.
  explicit HWNDMessageHandler(HWNDMessageHandlerDelegate* delegate);
  ~HWNDMessageHandler() override;

  void Init(HWND parent, const gfx::Rect& bounds);
  void Close();
  void CloseNow();

  gfx::Rect GetWindowBoundsInScreen() const;
  gfx::Rect GetClientAreaBoundsInScreen() const;
  gfx::Rect GetRestoredBounds() const;
  // This accounts for the case where the widget size is the client size.
  gfx::Rect GetClientAreaBounds() const;

  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const;

  // Sets the bounds of the HWND to |bounds_in_pixels|. If the HWND size is not
  // changed, |force_size_changed| determines if we should pretend it is.
  void SetBounds(const gfx::Rect& bounds_in_pixels, bool force_size_changed);

  void SetSize(const gfx::Size& size);
  void CenterWindow(const gfx::Size& size);

  void StackAbove(HWND other_hwnd);
  void StackAtTop();

  // Shows the window. If |show_state| is maximized, |pixel_restore_bounds| is
  // the bounds to restore the window to when going back to normal.
  void Show(ui::WindowShowState show_state,
            const gfx::Rect& pixel_restore_bounds);
  void Hide();

  void Maximize();
  void Minimize();
  void Restore();

  void Activate();
  void Deactivate();

  void SetAlwaysOnTop(bool on_top);

  bool IsVisible() const;
  bool IsActive() const;
  bool IsMinimized() const;
  bool IsMaximized() const;
  bool IsFullscreen() const;
  bool IsAlwaysOnTop() const;

  // Tells the HWND its client area has changed.
  void SendFrameChanged();

  void FlashFrame(bool flash);

  void ClearNativeFocus();

  FullscreenHandler* fullscreen_handler() { return fullscreen_handler_.get(); }

  // Returns true if the title changed.
  bool SetTitle(const std::u16string& title);

  void FrameTypeChanged();

  void SetFullscreen(bool fullscreen);

  // Updates the aspect ratio of the window.
  void SetAspectRatio(float aspect_ratio);

  // Updates the window style to reflect whether it can be resized or maximized.
  void SizeConstraintsChanged();

  void set_is_translucent(bool is_translucent) {
    is_translucent_ = is_translucent;
  }
  bool is_translucent() const { return is_translucent_; }

 private:
  // friend class ::views::test::DesktopWindowTreeHostWinTestApi;

  enum class DwmFrameState { kOff, kOn };

  // Overridden from WindowImpl:
  LRESULT OnWndProc(UINT message, WPARAM w_param, LPARAM l_param) override;

  // Returns a bitmask of auto-hide taskbar edges for |monitor|.
  int GetAppbarAutohideEdges(HMONITOR monitor);

  // Callback when the auto-hide taskbar edges may have changed.
  void OnAppbarAutohideEdgesChanged();

  // Can be called after the delegate has had the opportunity to set focus and
  // did not do so.
  void SetInitialFocus();

  // Called after the WM_ACTIVATE message has been processed by the default
  // windows procedure.
  void PostProcessActivateMessage(int activation_state,
                                  bool minimized,
                                  HWND window_gaining_or_losing_activation);

  // Enables disabled owner windows that may have been disabled due to this
  // window's modality.
  void RestoreEnabledIfNecessary();

  // Executes the specified SC_command.
  void ExecuteSystemMenuCommand(int command);

  // Start tracking all mouse events so that this window gets sent mouse leave
  // messages too.

  // Responds to the client area changing size, either at window creation time
  // or subsequently.
  void ClientAreaSizeChanged();

  // Returns true if |insets| was modified to define a custom client area for
  // the window, false if the default client area should be used. If false is
  // returned, |insets| is not modified.  |monitor| is the monitor this
  // window is on.  Normally that would be determined from the HWND, but
  // during WM_NCCALCSIZE Windows does not return the correct monitor for the
  // HWND, so it must be passed in explicitly (see HWNDMessageHandler::
  // OnNCCalcSize for more details).
  bool GetClientAreaInsets(gfx::Insets* insets, HMONITOR monitor) const;

  // Calls DefWindowProc, safely wrapping the call in a ScopedRedrawLock to
  // prevent frame flicker. DefWindowProc handling can otherwise render the
  // classic-look window title bar directly.
  LRESULT DefWindowProcWithRedrawLock(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param);

  // Stops ignoring SetWindowPos() requests (see below).
  void StopIgnoringPosChanges() { ignore_window_pos_changes_ = false; }

  // Returns whether Windows should help with frame rendering (i.e. we're using
  // the glass frame).
  bool IsFrameSystemDrawn() const;

  // Returns true if IsFrameSystemDrawn() and there's actually a frame to draw.
  bool HasSystemFrame() const;

  // Adds or removes the frame extension into client area with
  // DwmExtendFrameIntoClientArea.
  void SetDwmFrameExtension(DwmFrameState state);

  // Message Handlers ----------------------------------------------------------

  CR_BEGIN_MSG_MAP_EX(HWNDMessageHandler)
    // CustomFrameWindow hacks
    CR_MESSAGE_HANDLER_EX(WM_NCUAHDRAWCAPTION, OnNCUAHDrawCaption)
    CR_MESSAGE_HANDLER_EX(WM_NCUAHDRAWFRAME, OnNCUAHDrawFrame)

    // Vista and newer
    CR_MESSAGE_HANDLER_EX(WM_DWMCOMPOSITIONCHANGED, OnDwmCompositionChanged)

    // Win 8.1 and newer
    CR_MESSAGE_HANDLER_EX(WM_DPICHANGED, OnDpiChanged)

    CR_MESSAGE_HANDLER_EX(WM_WINDOWSIZINGFINISHED, OnWindowSizingFinished)

    // This list is in _ALPHABETICAL_ order! OR I WILL HURT YOU.
    CR_MSG_WM_ACTIVATEAPP(OnActivateApp)
    CR_MSG_WM_APPCOMMAND(OnAppCommand)
    CR_MSG_WM_CANCELMODE(OnCancelMode)
    CR_MSG_WM_CLOSE(OnClose)
    CR_MSG_WM_COMMAND(OnCommand)
    CR_MSG_WM_CREATE(OnCreate)
    CR_MSG_WM_DESTROY(OnDestroy)
    CR_MSG_WM_DISPLAYCHANGE(OnDisplayChange)
    CR_MSG_WM_ENTERMENULOOP(OnEnterMenuLoop)
    CR_MSG_WM_EXITMENULOOP(OnExitMenuLoop)
    CR_MSG_WM_ENTERSIZEMOVE(OnEnterSizeMove)
    CR_MSG_WM_ERASEBKGND(OnEraseBkgnd)
    CR_MSG_WM_EXITSIZEMOVE(OnExitSizeMove)
    CR_MSG_WM_GETMINMAXINFO(OnGetMinMaxInfo)
    CR_MSG_WM_INITMENU(OnInitMenu)
    CR_MSG_WM_KILLFOCUS(OnKillFocus)
    CR_MSG_WM_MOVE(OnMove)
    CR_MSG_WM_MOVING(OnMoving)
    CR_MESSAGE_HANDLER_EX(WM_NCACTIVATE, OnNCActivate)
    CR_MSG_WM_NCCALCSIZE(OnNCCalcSize)
    CR_MSG_WM_NCCREATE(OnNCCreate)
    CR_MSG_WM_NCHITTEST(OnNCHitTest)
    CR_MESSAGE_HANDLER_EX(WM_NCPAINT, OnNCPaint)
    CR_MSG_WM_PAINT(OnPaint)
    CR_MSG_WM_SETFOCUS(OnSetFocus)
    CR_MSG_WM_SETICON(OnSetIcon)
    CR_MSG_WM_SETTEXT(OnSetText)
    CR_MSG_WM_SETTINGCHANGE(OnSettingChange)
    CR_MSG_WM_SIZE(OnSize)
    CR_MSG_WM_SIZING(OnSizing)
    CR_MSG_WM_SYSCOMMAND(OnSysCommand)
    CR_MSG_WM_THEMECHANGED(OnThemeChanged)
    CR_MSG_WM_TIMECHANGE(OnTimeChange)
    CR_MSG_WM_WINDOWPOSCHANGED(OnWindowPosChanged)
    CR_MSG_WM_WINDOWPOSCHANGING(OnWindowPosChanging)
  CR_END_MSG_MAP()

  // Message Handlers.
  // This list is in _ALPHABETICAL_ order!
  // TODO(beng): Once this object becomes the WindowImpl, these methods can
  //             be made private.
  void OnActivateApp(BOOL active, DWORD thread_id);
  // TODO(beng): return BOOL is temporary until this object becomes a
  //             WindowImpl.
  BOOL OnAppCommand(HWND window, int command, WORD device, WORD keystate);
  void OnCancelMode();
  void OnClose();
  void OnCommand(UINT notification_code, int command, HWND window);
  LRESULT OnCreate(CREATESTRUCT* create_struct);
  void OnDestroy();
  void OnDisplayChange(UINT bits_per_pixel, const gfx::Size& screen_size);
  LRESULT OnDpiChanged(UINT msg, WPARAM w_param, LPARAM l_param);
  LRESULT OnDwmCompositionChanged(UINT msg, WPARAM w_param, LPARAM l_param);
  void OnEnterMenuLoop(BOOL from_track_popup_menu);
  void OnEnterSizeMove();
  LRESULT OnEraseBkgnd(HDC dc);
  void OnExitMenuLoop(BOOL is_shortcut_menu);
  void OnExitSizeMove();
  void OnGetMinMaxInfo(MINMAXINFO* minmax_info);
  void OnInitMenu(HMENU menu);
  void OnKillFocus(HWND focused_window);
  void OnMove(const gfx::Point& point);
  void OnMoving(UINT param, RECT* new_bounds);
  LRESULT OnNCActivate(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);
  LRESULT OnNCCreate(LPCREATESTRUCT lpCreateStruct);
  LRESULT OnNCHitTest(const gfx::Point& point);
  LRESULT OnNCPaint(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCUAHDrawCaption(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCUAHDrawFrame(UINT message, WPARAM w_param, LPARAM l_param);
  void OnPaint(HDC dc);
  void OnSetFocus(HWND last_focused_window);
  LRESULT OnSetIcon(UINT size_type, HICON new_icon);
  LRESULT OnSetText(const wchar_t* text);
  void OnSettingChange(UINT flags, const wchar_t* section);
  void OnSize(UINT param, const gfx::Size& size);
  void OnSizing(UINT param, RECT* rect);
  void OnSysCommand(UINT notification_code, const gfx::Point& point);
  void OnThemeChanged();
  void OnTimeChange();
  void OnWindowPosChanging(WINDOWPOS* window_pos);
  void OnWindowPosChanged(WINDOWPOS* window_pos);
  LRESULT OnWindowSizingFinished(UINT message, WPARAM w_param, LPARAM l_param);

  // Updates DWM frame to extend into client area if needed.
  void UpdateDwmFrame();

  // Handles WM_NCLBUTTONDOWN and WM_NCMOUSEMOVE messages on the caption.

  // Helper function for setting the bounds of the HWND. For more information
  // please refer to the SetBounds() function.
  void SetBoundsInternal(const gfx::Rect& bounds_in_pixels,
                         bool force_size_changed);

  // Checks if there is a full screen window on the same monitor as the
  // |window| which is becoming active. If yes then we reduce the size of the
  // fullscreen window by 1 px to ensure that maximized windows on the same
  // monitor don't draw over the taskbar.
  void CheckAndHandleBackgroundFullscreenOnMonitor(HWND window);

  // Provides functionality to reduce the bounds of the fullscreen window by 1
  // px on activation loss to a window on the same monitor.
  void OnBackgroundFullscreen();

  // Updates |rect| to adhere to the |aspect_ratio| of the window. |param|
  // refers to the edge of the window being sized.
  void SizeWindowToAspectRatio(UINT param, gfx::Rect* rect);

  // Get the cursor position, which may be mocked if running a test

  base::raw_ptr<HWNDMessageHandlerDelegate> delegate_;

  std::unique_ptr<FullscreenHandler> fullscreen_handler_;

  // Whether all ancestors have been enabled. This is only used if is_modal_ is
  // true.
  bool restored_enabled_;

  // The aspect ratio for the window. This is only used for sizing operations
  // for the non-client area.
  absl::optional<float> aspect_ratio_;

  // The current DPI.
  int dpi_;

  // Whether EnableNonClientDpiScaling was called successfully with this window.
  // This flag exists because EnableNonClientDpiScaling must be called during
  // WM_NCCREATE and EnableChildWindowDpiMessage is called after window
  // creation. We don't want to call both, so this helps us determine if a call
  // to EnableChildWindowDpiMessage is necessary.
  bool called_enable_non_client_dpi_scaling_;

  // Event handling ------------------------------------------------------------

  // The flags currently being used with TrackMouseEvent to track mouse
  // messages. 0 if there is no active tracking. The value of this member is
  // used when tracking is canceled.

  // Set to true when the user presses the right mouse button on the caption
  // area. We need this so we can correctly show the context menu on mouse-up.

  // The set of touch devices currently down.

  // Window resizing -----------------------------------------------------------

  // When true, this flag makes us discard incoming SetWindowPos() requests that
  // only change our position/size.  (We still allow changes to Z-order,
  // activation, etc.)
  bool ignore_window_pos_changes_;

  // Keeps track of the last size type param received from a WM_SIZE message.
  UINT last_size_param_ = SIZE_RESTORED;

  // The last-seen monitor containing us, and its rect and work area.  These are
  // used to catch updates to the rect and work area and react accordingly.
  HMONITOR last_monitor_;
  gfx::Rect last_monitor_rect_, last_work_area_;

  // True the first time nccalc is called on a sizable widget
  bool is_first_nccalc_;

  // If > 0 indicates a menu is running (we're showing a native menu).
  int menu_depth_;

  // Is DWM composition currently enabled?
  // Note: According to MSDN docs for DwmIsCompositionEnabled(), this is always
  // true starting in Windows 8.
  bool dwm_composition_enabled_;

  // True if HandleWindowSizeChanging has been called in the delegate, but not
  // HandleClientSizeChanged.
  bool sent_window_size_changing_;

  // This is used to keep track of whether a WM_WINDOWPOSCHANGED has
  // been received after the WM_WINDOWPOSCHANGING.
  uint32_t current_window_size_message_ = 0;

  // Set to true when we return a UIA object. Determines whether we need to
  // call UIA to clean up object references on window destruction.
  // This is important to avoid triggering a cross-thread COM call which could
  // cause re-entrancy during teardown. https://crbug.com/1087553
  bool did_return_uia_object_;

  // The location where the user clicked on the caption.

  // Set to true if the window is a background fullscreen window, i.e a
  // fullscreen window which lost activation. Defaults to false.
  bool background_fullscreen_hack_;

  // True if the window should have no border and its contents should be
  // partially or fully transparent.
  bool is_translucent_ = false;

  // True if the window should process WM_POINTER for touch events and
  // not WM_TOUCH events.
  bool pointer_events_for_touch_ = false;

  // True if DWM frame should be cleared on next WM_ERASEBKGND message.  This is
  // necessary to avoid white flashing in the titlebar area around the
  // minimize/maximize/close buttons.  Clearing the frame on every WM_ERASEBKGND
  // message causes black flickering in the titlebar region so we do it on for
  // the first message after frame type changes.
  bool needs_dwm_frame_clear_ = true;

  // True if is handling mouse WM_INPUT messages.

  // True if we're displaying the system menu on the title bar. If we are,
  // then we want to ignore right mouse clicks instead of bringing up a
  // context menu.
  bool handling_mouse_menu_ = false;

  // This is set to true when we call ShowWindow(SC_RESTORE), in order to
  // call HandleWindowMinimizedOrRestored() when we get a WM_ACTIVATE message.
  bool notify_restore_on_activate_ = false;

  // This is a map of the HMONITOR to full screeen window instance. It is safe
  // to keep a raw pointer to the HWNDMessageHandler instance as we track the
  // window destruction and ensure that the map is cleaned up.
  using FullscreenWindowMonitorMap = std::map<HMONITOR, HWNDMessageHandler*>;
  static base::LazyInstance<FullscreenWindowMonitorMap>::DestructorAtExit
      fullscreen_monitor_map_;

  // How many pixels the window is expected to grow from OnWindowPosChanging().
  // Used to fill the newly exposed pixels black in OnPaint() before the
  // browser compositor is able to redraw at the new window size.
  gfx::Size exposed_pixels_;

  // The WeakPtrFactories below (one inside the
  // CR_MSG_MAP_CLASS_DECLARATIONS macro and autohide_factory_) must
  // occur last in the class definition so they get destroyed last.

  CR_MSG_MAP_CLASS_DECLARATIONS(HWNDMessageHandler)

  // The factory used to lookup appbar autohide edges.
  base::WeakPtrFactory<HWNDMessageHandler> autohide_factory_{this};
};

}  // namespace ui

#endif  // LYNXTRON_SHELL_UI_PLATFORM_WINDOW_WIN_HWND_MESSAGE_HANDLER_H_
