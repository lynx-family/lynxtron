// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_API_NATIVE_WINDOW_MAC_H_
#define LYNXTRON_SHELL_API_NATIVE_WINDOW_MAC_H_

#import <Cocoa/Cocoa.h>

#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

// #include "base/apple/scoped_nsobject.h"
// #include "base/memory/raw_ptr.h"
#include "shell/app/native_window.h"
#include "shell/ui/gfx/geometry/rect.h"

@class LynxNSWindow;
@class LynxNSWindowDelegate;

namespace lynxtron {

class NativeWindowMac : public NativeWindow {
 public:
  NativeWindowMac(const gin_helper::Dictionary& options, NativeWindow* parent);
  ~NativeWindowMac() override;

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
  // SkColor GetBackgroundColor() override;
  void SetContentSizeConstraints(
      const SizeConstraints& size_constraints) override;

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
  std::string GetAlwaysOnTopLevel() override;
  // void SetAlwaysOnTop(ui::ZOrderLevel z_order,
  //                    const std::string& level,
  //                    int relativeLevel) override;
  // ui::ZOrderLevel GetZOrderLevel() override;
  void Center() override;
  void SetTitle(const std::string& title) override;
  std::string GetTitle() const override;
  void FlashFrame(bool flash) override;
  void SetSkipTaskbar(bool skip) override;
  void SetExcludedFromShownWindowsMenu(bool excluded) override;
  bool IsExcludedFromShownWindowsMenu() override;
  void SetSimpleFullScreen(bool simple_fullscreen) override;
  bool IsSimpleFullScreen() override;
  void SetKiosk(bool kiosk) override;
  bool IsKiosk() const override;
  // void SetBackgroundColor(SkColor color) override;
  void SetHasShadow(bool has_shadow) override;
  bool HasShadow() override;
  void SetOpacity(const double opacity) override;
  double GetOpacity() override;
  // void SetIgnoreMouseEvents(bool ignore, bool forward) override;
  void SetContentProtection(bool enable) override;
  void SetFocusable(bool focusable) override;
  bool IsFocusable() const override;
  // void SetMenu(ElectronMenuModel* menu_model) override;
  // void AddBrowserView(NativeBrowserView* browser_view) override;
  // void RemoveBrowserView(NativeBrowserView* browser_view) override;
  // void SetTopBrowserView(NativeBrowserView* browser_view) override;
  void SetParentWindow(NativeWindow* parent) override;
  // gfx::NativeView GetNativeView() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  // void SetOverlayIcon(const gfx::Image& overlay,
  //                    const std::string& description) override;
  // void SetProgressBar(double progress, const ProgressState state) override;

  void SetVisibleOnAllWorkspaces(bool visible,
                                 bool visibleOnFullScreen,
                                 bool skipTransformProcessType) override;

  bool IsVisibleOnAllWorkspaces() override;

  void SetWindowButtonVisibility(bool visible) override;
  bool GetWindowButtonVisibility() const override;

  void SetTrafficLightPosition(std::optional<gfx::Point> position) override;
  std::optional<gfx::Point> GetTrafficLightPosition() const override;
  void RedrawTrafficLights() override;
  void UpdateFrame() override;

  void SetActive(bool is_key) override;
  bool IsActive() const override;

  // content::DesktopMediaID GetDesktopMediaID() const override;
  // gfx::AcceleratedWidget GetAcceleratedWidget() const override;
  NativeWindowHandle GetNativeWindowHandle() const override;

  gfx::Rect ContentBoundsToWindowBounds(const gfx::Rect& bounds) const override;
  gfx::Rect WindowBoundsToContentBounds(const gfx::Rect& bounds) const override;

  void UpdateWindowOriginalFrame();

  enum class FullScreenTransitionState { ENTERING, EXITING, NONE };

  bool HasStyleMask(NSUInteger flag) const;
  void SetStyleMask(bool on, NSUInteger flag);
  void SetCollectionBehavior(bool on, NSUInteger flag);
  void SetWindowLevel(int level);

  bool always_simple_fullscreen() const { return always_simple_fullscreen_; }

  // Handle fullscreen transitions.
  void set_fullscreen_transition_state(FullScreenTransitionState state) {
    fullscreen_transition_state_ = state;
  }
  FullScreenTransitionState fullscreen_transition_state() const {
    return fullscreen_transition_state_;
  }

  NSRect default_frame_for_zoom() const { return default_frame_for_zoom_; }
  void set_default_frame_for_zoom(NSRect frame) {
    default_frame_for_zoom_ = frame;
  }

  void SetAlwaysOnTop(ui::ZOrderLevel z_order,
                      const std::string& level = "floating",
                      int relativeLevel = 0) override;
  ui::ZOrderLevel GetZOrderLevel() const override;

  void SetBorderless(bool borderless);

 protected:
  std::queue<bool> pending_transitions_;
  FullScreenTransitionState fullscreen_transition_state_ =
      FullScreenTransitionState::NONE;

 private:
  void InternalSetParentWindow(NativeWindow* parent, bool attach);
  void SetForwardMouseMessages(bool forward);
  void InternalSetWindowButtonVisibility(bool visible);

  LynxNSWindow* __strong window_;
  LynxNSWindowDelegate* __strong window_delegate_;

  NSInteger attention_request_id_ = 0;  // identifier from requestUserAttention

  bool is_kiosk_ = false;

  std::optional<gfx::Point> traffic_light_position_;

  // The presentation options before entering kiosk mode.
  NSApplicationPresentationOptions kiosk_options_;

  // Controls the position and visibility of window buttons.
  // base::scoped_nsobject<WindowButtonsProxy> buttons_proxy_;

  // Maximizable window state; necessary for persistence through redraws.
  bool maximizable_ = true;

  bool user_set_bounds_maximized_ = false;

  // Simple (pre-Lion) Fullscreen Settings
  bool always_simple_fullscreen_ = false;
  bool is_simple_fullscreen_ = false;
  bool was_maximizable_ = false;
  bool was_movable_ = false;

  bool is_active_ = false;
  NSRect original_frame_;
  NSInteger original_level_;
  NSUInteger simple_fullscreen_mask_;
  NSRect default_frame_for_zoom_;

  // The presentation options before entering simple fullscreen mode.
  NSApplicationPresentationOptions simple_fullscreen_options_;
  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;

  // DISALLOW_COPY_AND_ASSIGN(NativeWindowMac);
};

}  // namespace lynxtron

#endif  // LYNX_NATIVE_WINDOW_VIEWS_MAC_H_
