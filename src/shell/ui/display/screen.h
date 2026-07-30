// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_DISPLAY_SCREEN_H_
#define LYNXTRON_SHELL_UI_DISPLAY_SCREEN_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "ui/display/display.h"
#include "ui/gfx/native_ui_types.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace display {
class DisplayObserver;
// enum class TabletState;

// A utility class for getting various info about screen size, displays,
// cursor position, etc.
//
// Also, can notify DisplayObservers about global workspace changes. The
// availability of that functionality depends on a platform.
//
// Note that this class does not represent an individual display connected to a
// computer -- see the Display class for that. A single Screen object exists
// regardless of the number of connected displays.
class Screen {
 public:
  Screen();

  Screen(const Screen&) = delete;
  Screen& operator=(const Screen&) = delete;

  virtual ~Screen();

  // Retrieves the single Screen object; this may be null if it's not already
  // created, except for IOS where it creates a native screen instance
  // automatically. On ChromeOS ash the return value is only null on startup.
  static Screen* Get();

  // Returns whether a Screen singleton exists or not.
  static bool HasScreen();

  // [Deprecated] as a public method. Do not use this.
  // Sets the global screen. Returns the previously installed screen, if any.
  // NOTE: this does not take ownership of |screen|. Tests must be sure to reset
  // any state they install.
  static Screen* SetScreenInstance(Screen* instance,
                                   const base::Location& location = FROM_HERE);

  // Returns the current absolute position of the mouse pointer.
  virtual gfx::Point GetCursorScreenPoint() = 0;

  // Allows tests to override the cursor point location on the screen.
  virtual void SetCursorScreenPointForTesting(const gfx::Point& point);

  // Returns the number of displays.  Mirrored displays are excluded; this
  // method is intended to return the number of distinct, usable displays.
  // The value returned must be at least 1, as GetAllDisplays returns a fake
  // display if there are no displays in the system.
  virtual int GetNumDisplays() const = 0;

  // Returns the list of displays that are currently available.
  // Screen subclasses must return at least one Display, even if it is fake.
  virtual const std::vector<Display>& GetAllDisplays() const = 0;

  // Returns the display nearest the specified window.
  // If the window is NULL or the window is not rooted to a display this will
  // return the primary display.
  //
  // Warning: When determining which scale factor to use for a given native
  // window, use `GetPreferredScaleFactorForWindow` instead, as it properly
  // supports system-controlled per-window scaling, such as Wayland.
  virtual Display GetDisplayNearestWindow(gfx::NativeWindow window) const = 0;

  // Returns the display nearest the specified DIP |point|.
  virtual Display GetDisplayNearestPoint(const gfx::Point& point) const = 0;

  // Returns the display that most closely intersects the DIP rect |match_rect|.
  virtual Display GetDisplayMatching(const gfx::Rect& match_rect) const = 0;

  // Returns the primary display. It is guaranteed that this will return a
  // display with a valid display ID even if there is no display connected.
  // A real display will be reported via DisplayObserver when it is connected.
  virtual Display GetPrimaryDisplay() const = 0;

  // Returns a suggested display to use when creating a new window. On most
  // platforms just returns the primary display.
  Display GetDisplayForNewWindows() const;

  // Sets the suggested display to use when creating a new window.
  virtual void SetDisplayForNewWindows(int64_t display_id);

  // Returns whether the screensaver is currently running.
  virtual bool IsScreenSaverActive() const;

  // Calculates idle time.
  virtual base::TimeDelta CalculateIdleTime() const;

  // Adds/Removes display observers.
  virtual void AddObserver(DisplayObserver* observer) = 0;
  virtual void RemoveObserver(DisplayObserver* observer) = 0;

  // Returns true if the display with |display_id| is found and returns that
  // display in |display|. Otherwise returns false and |display| remains
  // untouched.
  bool GetDisplayWithDisplayId(int64_t display_id, Display* display) const;

  virtual void SetPanelRotationForTesting(int64_t display_id,
                                          Display::Rotation rotation);

  // Depending on a platform, a client can listen to global workspace changes
  // by implementing and setting self as a DisplayObserver. It is also possible
  // to get current workspace through the GetCurrentWorkspace method.
  virtual std::string GetCurrentWorkspace();

  // Returns true when running in headless mode.
  virtual bool IsHeadless() const;

 protected:
  void set_shutdown(bool shutdown) { shutdown_ = shutdown; }
  int64_t display_id_for_new_windows() const {
    return display_id_for_new_windows_;
  }

 private:
  friend class ScopedDisplayForNewWindows;

  // Used to temporarily override the value from SetDisplayForNewWindows() by
  // creating an instance of ScopedDisplayForNewWindows. Call with
  // |kInvalidDisplayId| to unset.
  void SetScopedDisplayForNewWindows(int64_t display_id);

  static gfx::NativeWindow GetWindowForView(gfx::NativeView view);

  // A flag indicates that the instance is a special one used during shutdown.
  bool shutdown_ = false;

  int64_t display_id_for_new_windows_;
  int64_t scoped_display_id_for_new_windows_ = display::kInvalidDisplayId;
};

#if BUILDFLAG(IS_APPLE)
Screen* CreateNativeScreen();

// ScopedNativeScreen creates a native screen if there is no screen created yet
// (e.g. by a unit test).
class ScopedNativeScreen final {
 public:
  explicit ScopedNativeScreen(const base::Location& location = FROM_HERE);
  ScopedNativeScreen(const ScopedNativeScreen&) = delete;
  ScopedNativeScreen& operator=(const ScopedNativeScreen&) = delete;
  ~ScopedNativeScreen();

 private:
  std::unique_ptr<Screen> screen_;
};

#endif

}  // namespace display

#endif  // LYNXTRON_SHELL_UI_DISPLAY_SCREEN_H_
