// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_NATIVE_WINDOW_MAC_H_
#define LYNXTRON_SHELL_API_NATIVE_WINDOW_MAC_H_

#import <Cocoa/Cocoa.h>

#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "shell/app/native_window.h"
#include "shell/ui/gfx/geometry/rect.h"

@class LynxNSWindow;
@class LynxNSWindowDelegate;
@class WindowButtonsProxy;

namespace lynxtron {

class NativeWindowMac : public NativeWindow {
 public:
  enum class VisualEffectState {
    kFollowWindow,
    kActive,
    kInactive,
  };

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
  void SetSizeConstraints(const SizeConstraints& window_constraints) override;
  void SetBounds(const gfx::Rect& bounds, bool animate) override;
  gfx::Rect GetBounds() const override;
  gfx::Size GetSize() const override;
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
  std::string GetAlwaysOnTopLevel() override;
  void Center() override;
  void SetTitle(const std::string& title) override;
  std::string GetTitle() const override;
  void FlashFrame(bool flash) override;
  void SetSkipTaskbar(bool skip) override;
  void SetExcludedFromShownWindowsMenu(bool excluded) override;
  bool IsExcludedFromShownWindowsMenu() override;
  void SetSimpleFullScreen(bool simple_fullscreen) override;
  bool IsSimpleFullScreen() override;
  void SetBackgroundColor(SkColor background_color) override;
  SkColor GetBackgroundColor() const override;
  void SetHasShadow(bool has_shadow) override;
  bool HasShadow() override;
  void SetOpacity(const double opacity) override;
  double GetOpacity() override;
  bool IsHiddenInMissionControl() const override;
  void SetHiddenInMissionControl(bool hidden) override;
  void SetFocusable(bool focusable) override;
  bool IsFocusable() const override;
  void SetVibrancy(const std::string& type, int duration) override;
  bool HasVibrancyView() const override;
  std::string GetVisualEffectStateForTesting() const override;
  std::string GetNativeVisualEffectStateForTesting() const override;
  void SetParentWindow(NativeWindow* parent) override;
  gfx::NativeWindow GetNativeWindow() const override;
  void SetProgressBar(double progress, const ProgressState state) override;

  void SetVisibleOnAllWorkspaces(bool visible,
                                 bool visibleOnFullScreen,
                                 bool skipTransformProcessType) override;

  bool IsVisibleOnAllWorkspaces() override;
  void SetAutoHideCursor(bool auto_hide) override;
  void AddTabbedWindow(NativeWindow* window) override;
  void SelectPreviousTab() override;
  void SelectNextTab() override;
  void ShowAllTabs() override;
  void MergeAllWindows() override;
  void MoveTabToNewWindow() override;
  void ToggleTabBar() override;

  void SetWindowButtonVisibility(bool visible) override;
  bool GetWindowButtonVisibility() const override;

  void SetTrafficLightPosition(std::optional<gfx::Point> position) override;
  std::optional<gfx::Point> GetTrafficLightPosition() const override;
  void RedrawTrafficLights() override;
  void UpdateFrame() override;

  void SetActive(bool is_key) override;
  bool IsActive() const override;
  std::optional<std::string> GetTabbingIdentifier() const override;
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
  void HandlePendingFullscreenTransitions();
  bool HandleDeferredClose();

  NSRect default_frame_for_zoom() const { return default_frame_for_zoom_; }
  void set_default_frame_for_zoom(NSRect frame) {
    default_frame_for_zoom_ = frame;
  }

  void SetAlwaysOnTop(ui::ZOrderLevel z_order,
                      const std::string& level = "floating",
                      int relativeLevel = 0) override;
  ui::ZOrderLevel GetZOrderLevel() const override;

  void SetBorderless(bool borderless);

  // Helpers for macOS window state transitions (aligned with Electron
  // behavior). Used to avoid AppKit quirks during minimize/restore and
  // visibility changes.
  void DetachChildren();
  void AttachChildren();
  void HideTrafficLights();
  void RestoreTrafficLights();
  void SyncWindowVisibilityState();
  void set_restore_from_maximize_pending(bool pending) {
    restore_from_maximize_pending_ = pending;
  }
  bool consume_restore_from_maximize_pending() {
    const bool pending = restore_from_maximize_pending_;
    restore_from_maximize_pending_ = false;
    return pending;
  }

 protected:
  std::queue<bool> pending_transitions_;
  FullScreenTransitionState fullscreen_transition_state_ =
      FullScreenTransitionState::NONE;
  bool deferred_close_ = false;

 private:
  enum class TitleBarStyle {
    kNormal,
    kHidden,
    kHiddenInset,
    kCustomButtonsOnHover,
  };

  void CloseDisableOverlay();
  void CloseAttachedSheets();
  void InternalSetParentWindow(NativeWindow* parent, bool attach);
  void InternalSetWindowButtonVisibility(bool visible);
  void PerformTabAction(SEL selector);
  bool wants_to_be_visible() const { return wants_to_be_visible_; }
  void set_wants_to_be_visible(bool wants_to_be_visible) {
    wants_to_be_visible_ = wants_to_be_visible;
  }
  void UpdateVibrancyState();

  LynxNSWindow* __strong window_;
  LynxNSWindowDelegate* __strong window_delegate_;

  NSInteger attention_request_id_ = 0;  // identifier from requestUserAttention

  std::optional<gfx::Point> traffic_light_position_;

  // Controls the position and visibility of window buttons.
  WindowButtonsProxy* __strong buttons_proxy_ = nil;
  bool traffic_lights_hidden_ = false;

  // Children that were visible prior to miniaturize and were ordered out in
  // DetachChildren(). Stored so they can be shown again on restore.
  std::vector<NSWindow*> minimized_visible_children_;

  NSWindow* __strong disabled_window_ = nil;

  TitleBarStyle title_bar_style_ = TitleBarStyle::kNormal;

  bool user_set_bounds_maximized_ = false;

  // Simple (pre-Lion) Fullscreen Settings
  bool always_simple_fullscreen_ = false;
  bool is_simple_fullscreen_ = false;
  bool was_maximizable_ = false;
  bool was_movable_ = false;

  bool is_active_ = false;
  bool wants_to_be_visible_ = false;
  // Cache the last reported visibility to avoid duplicate events.
  bool last_reported_visible_ = false;
  bool restore_from_maximize_pending_ = false;
  NSRect original_frame_;
  NSInteger original_level_;
  NSUInteger simple_fullscreen_mask_;
  NSRect default_frame_for_zoom_;

  // The presentation options before entering simple fullscreen mode.
  NSApplicationPresentationOptions simple_fullscreen_options_;
  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;
  VisualEffectState visual_effect_state_ = VisualEffectState::kFollowWindow;

  // DISALLOW_COPY_AND_ASSIGN(NativeWindowMac);
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_NATIVE_WINDOW_MAC_H_
