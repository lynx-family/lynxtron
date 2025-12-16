// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/screen.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
// #include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/display/display.h"
// #include "ui/display/tablet_state.h"
#include "ui/display/types/display_constants.h"
// #include "ui/gfx/geometry/rect.h"

namespace display {

namespace {

Screen* g_screen;

}  // namespace

Screen::Screen() : display_id_for_new_windows_(kInvalidDisplayId) {}

Screen::~Screen() = default;

// static
Screen* Screen::Get() {
  return g_screen;
}

// static
Screen* Screen::SetScreenInstance(Screen* instance,
                                  const base::Location& location) {
  // Do not allow screen instance override. The screen object has a lot of
  // states, such as current display settings as well as observers, and safely
  // transferring these to new screen implementation is very difficult and not
  // safe.  If you hit the DCHECK in a test, please look for other examples that
  // that set a test screen instance in the setup process.
  DCHECK(!g_screen || !instance || (instance && instance->shutdown_))
      << "fail=" << location.ToString();
  return std::exchange(g_screen, instance);
}

// static
bool Screen::HasScreen() {
  return !!g_screen;
}

void Screen::SetCursorScreenPointForTesting(const gfx::Point& point) {
  NOTIMPLEMENTED_LOG_ONCE();
}

Display Screen::GetDisplayForNewWindows() const {
  Display display;
  // Scoped value can override if it is set.
  if (scoped_display_id_for_new_windows_ != kInvalidDisplayId &&
      GetDisplayWithDisplayId(scoped_display_id_for_new_windows_, &display)) {
    return display;
  }

  if (GetDisplayWithDisplayId(display_id_for_new_windows_, &display)) {
    return display;
  }

  // Fallback to primary display.
  return GetPrimaryDisplay();
}

void Screen::SetDisplayForNewWindows(int64_t display_id) {
  // GetDisplayForNewWindows() handles invalid display ids.
  display_id_for_new_windows_ = display_id;
}

bool Screen::IsScreenSaverActive() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

base::TimeDelta Screen::CalculateIdleTime() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return base::Seconds(0);
}

bool Screen::GetDisplayWithDisplayId(int64_t display_id,
                                     Display* display) const {
  for (const Display& display_in_list : GetAllDisplays()) {
    if (display_in_list.id() == display_id) {
      *display = display_in_list;
      return true;
    }
  }
  return false;
}

void Screen::SetPanelRotationForTesting(int64_t display_id,
                                        Display::Rotation rotation) {
  // Not implemented.
  DCHECK(false);
}

std::string Screen::GetCurrentWorkspace() {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

// std::optional<float> Screen::GetPreferredScaleFactorForView(
//     gfx::NativeView view) const {
//   return GetPreferredScaleFactorForWindow(GetWindowForView(view));
// }

bool Screen::IsHeadless() const {
  return false;
}

void Screen::SetScopedDisplayForNewWindows(int64_t display_id) {
  if (display_id == scoped_display_id_for_new_windows_) {
    return;
  }
  // Only allow set and clear, not switch.
  DCHECK(display_id == kInvalidDisplayId ^
         scoped_display_id_for_new_windows_ == kInvalidDisplayId)
      << "display_id=" << display_id << ", scoped_display_id_for_new_windows_="
      << scoped_display_id_for_new_windows_;
  scoped_display_id_for_new_windows_ = display_id;
}

#if BUILDFLAG(IS_APPLE)

ScopedNativeScreen::ScopedNativeScreen(const base::Location& location) {
  if (!Screen::HasScreen()) {
    screen_ = base::WrapUnique(CreateNativeScreen());
    Screen::SetScreenInstance(screen_.get(), location);
  }
}

ScopedNativeScreen::~ScopedNativeScreen() {
  if (screen_) {
    DCHECK_EQ(screen_.get(), Screen::Get());
    Screen::SetScreenInstance(nullptr);
    screen_.reset();
  }
}

#endif

}  // namespace display
