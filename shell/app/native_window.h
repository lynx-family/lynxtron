// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_APP_NATIVE_WINDOW_H_
#define LYNXTRON_SHELL_APP_NATIVE_WINDOW_H_

#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "shell/app/native_window_observer.h"
#include "shell/common/size_constraints.h"
#include "shell/ui/base/ui_base_types.h"
#include "shell/ui/gfx/geometry/rect.h"
#include "shell/ui/gfx/native_ui_types.h"

namespace base {
class DictionaryValue;
}

namespace content {
struct NativeWebKeyboardEvent;
}

namespace gfx {
class Image;
class Point;
class Rect;
enum class ResizeEdge;
class Size;
}  // namespace gfx

namespace gin_helper {
class Dictionary;
class PersistentDictionary;
}  // namespace gin_helper

#if !defined(__OBJC__)
class NSView;
#endif
namespace lynxtron {
class LynxtronMenuModel;
class NativeBrowserView;

#if BUILDFLAG(IS_MAC)
using NativeWindowHandle = NSView*;
#else
using NativeWindowHandle = HWND;
#endif

class NativeWindow : public base::SupportsUserData {
 public:
  ~NativeWindow() override;

  // disable copy
  NativeWindow(const NativeWindow&) = delete;
  NativeWindow& operator=(const NativeWindow&) = delete;

  // Create window, the caller is responsible for
  // managing the window's live.
  static NativeWindow* Create(const gin_helper::Dictionary& options,
                              NativeWindow* parent = nullptr);

  virtual void IncrementChildModals() {}
  virtual void DecrementChildModals() {}

  void InitFromOptions(const gin_helper::Dictionary& options);

  virtual void Close() = 0;
  virtual void CloseImmediately() = 0;
  virtual bool IsClosed() const;
  virtual void Focus(bool focus) = 0;
  virtual bool IsFocused() = 0;
  virtual void Show() = 0;
  virtual void ShowInactive() = 0;
  virtual void Hide() = 0;
  virtual bool IsVisible() = 0;
  virtual bool IsEnabled() = 0;
  virtual void SetEnabled(bool enable) = 0;
  virtual void Maximize() = 0;
  virtual void Unmaximize() = 0;
  virtual bool IsMaximized() const = 0;
  virtual void Minimize() = 0;
  virtual void Restore() = 0;
  virtual bool IsMinimized() const = 0;
  virtual void SetFullScreen(bool fullscreen) = 0;
  virtual bool IsFullscreen() const = 0;
  virtual void SetBounds(const gfx::Rect& bounds, bool animate) = 0;
  virtual gfx::Rect GetBounds() const = 0;
  void SetShape(const std::vector<gfx::Rect>& rects);
  virtual void SetSize(const gfx::Size& size, bool animate);
  virtual gfx::Size GetSize() const;
  virtual float GetDevicePixelRatio() const;
  virtual void SetPosition(const gfx::Point& position, bool animate);
  virtual gfx::Point GetPosition() const;
  // virtual void SetContentSize(const gfx::Size& size, bool animate);
  // virtual gfx::Size GetContentSize() const;
  // virtual void SetContentBounds(const gfx::Rect& bounds, bool animate);
  // virtual gfx::Rect GetContentBounds() const;
  virtual bool IsNormal() const;
  virtual gfx::Rect GetNormalBounds() const = 0;
  virtual void SetSizeConstraints(const SizeConstraints& window_constraints);
  virtual SizeConstraints GetSizeConstraints() const;

  void SetMinimumSize(const gfx::Size& size);
  [[nodiscard]] gfx::Size GetMinimumSize() const;

  void SetMaximumSize(const gfx::Size& size);
  [[nodiscard]] gfx::Size GetMaximumSize() const;

  [[nodiscard]] gfx::Size GetContentMinimumSize() const;
  [[nodiscard]] gfx::Size GetContentMaximumSize() const;

  virtual void SetSheetOffset(const double offsetX, const double offsetY);
  virtual double GetSheetOffsetX();
  virtual double GetSheetOffsetY();
  virtual void SetResizable(bool resizable) = 0;
  virtual void MoveTop() = 0;
  virtual bool IsResizable() const = 0;
  virtual void SetMovable(bool movable) = 0;
  virtual bool IsMovable() const = 0;
  virtual void SetMinimizable(bool minimizable) = 0;
  virtual bool IsMinimizable() const = 0;
  virtual void SetMaximizable(bool maximizable) = 0;
  virtual bool IsMaximizable() const = 0;
  virtual void SetFullScreenable(bool fullscreenable) = 0;
  virtual bool IsFullScreenable() const = 0;
  virtual void SetClosable(bool closable) = 0;
  virtual bool IsClosable() const = 0;
  virtual void SetAlwaysOnTop(ui::ZOrderLevel z_order,
                              const std::string& level = "floating",
                              int relativeLevel = 0) = 0;
  virtual ui::ZOrderLevel GetZOrderLevel() const = 0;
  virtual void Center() = 0;
  virtual void SetTitle(const std::string& title) = 0;
  virtual std::string GetTitle() const = 0;
#if BUILDFLAG(IS_MAC)
  virtual std::string GetAlwaysOnTopLevel() = 0;
  virtual void SetActive(bool is_key) = 0;
  virtual bool IsActive() const = 0;
  virtual std::optional<std::string> GetTabbingIdentifier() const;
#endif

  // Ability to augment the window title for the screen readers.
  void SetAccessibleTitle(const std::string& title);
  std::string GetAccessibleTitle();

  virtual void FlashFrame(bool flash) = 0;
  virtual void SetSkipTaskbar(bool skip) = 0;
  virtual void SetExcludedFromShownWindowsMenu(bool excluded) = 0;
  virtual bool IsExcludedFromShownWindowsMenu() = 0;
  virtual void SetSimpleFullScreen(bool simple_fullscreen) = 0;
  virtual bool IsSimpleFullScreen() = 0;
  virtual void SetKiosk(bool kiosk) = 0;
  virtual bool IsKiosk() const = 0;
  virtual void SetHasShadow(bool has_shadow) = 0;
  virtual bool HasShadow() = 0;
  virtual void SetOpacity(const double opacity) = 0;
  virtual double GetOpacity() = 0;
  virtual void SetRepresentedFilename(const std::string& filename);
  virtual std::string GetRepresentedFilename();
  // virtual void SetIgnoreMouseEvents(bool ignore, bool forward) = 0;
  virtual void SetContentProtection(bool enable) = 0;
  virtual void SetFocusable(bool focusable);
  virtual bool IsFocusable() const;
  virtual void SetParentWindow(NativeWindow* parent);
#if BUILDFLAG(IS_MAC)
  virtual gfx::NativeWindow GetNativeWindow() const = 0;
#endif
  // virtual gfx::AcceleratedWidget GetAcceleratedWidget() const = 0;
  virtual NativeWindowHandle GetNativeWindowHandle() const = 0;

  // Taskbar/Dock APIs.
  enum class ProgressState {
    kNone,           // no progress, no marking
    kIndeterminate,  // progress, indeterminate
    kError,          // progress, errored (red)
    kPaused,         // progress, paused (yellow)
    kNormal,         // progress, not marked (green)
  };

  // Workspace APIs.
  virtual void SetVisibleOnAllWorkspaces(
      bool visible,
      bool visibleOnFullScreen = false,
      bool skipTransformProcessType = false) = 0;

  virtual bool IsVisibleOnAllWorkspaces() = 0;

  virtual void SetAutoHideCursor(bool auto_hide);

  // Vibrancy API
  virtual void SetVibrancy(const std::string& type, int duration);

  // Traffic Light API
#if BUILDFLAG(IS_MAC)
  virtual void SetWindowButtonVisibility(bool visible) = 0;
  virtual bool GetWindowButtonVisibility() const = 0;
  virtual void SetTrafficLightPosition(std::optional<gfx::Point> position) = 0;
  virtual std::optional<gfx::Point> GetTrafficLightPosition() const = 0;
  virtual void RedrawTrafficLights() = 0;
  virtual void UpdateFrame() = 0;
  virtual bool IsHiddenInMissionControl() const = 0;
  virtual void SetHiddenInMissionControl(bool hidden) = 0;
#endif

  // Touchbar API
  virtual void SetTouchBar(std::vector<gin_helper::PersistentDictionary> items);
  virtual void RefreshTouchBarItem(const std::string& item_id);
  virtual void SetEscapeTouchBarItem(gin_helper::PersistentDictionary item);
  // Toggle the menu bar.
  virtual void SetAutoHideMenuBar(bool auto_hide);
  virtual bool IsMenuBarAutoHide();
  virtual bool IsMenuBarVisible();

  // Set the aspect ratio when resizing window.
  double GetAspectRatio() const;
  gfx::Size GetAspectRatioExtraSize();
  virtual void SetAspectRatio(double aspect_ratio, const gfx::Size& extra_size);

  // File preview APIs.
  // virtual void PreviewFile(const std::string& path,
  //                          const std::string& display_name);
  // virtual void CloseFilePreview();

  // virtual void SetGTKDarkThemeEnabled(bool use_dark_theme) {}

  // Converts between content bounds and window bounds.
  // virtual gfx::Rect ContentBoundsToWindowBounds(
  //     const gfx::Rect& bounds) const = 0;
  // virtual gfx::Rect WindowBoundsToContentBounds(
  //     const gfx::Rect& bounds) const = 0;

  base::WeakPtr<NativeWindow> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Public API used by platform-dependent delegates and observers to send UI
  // related notifications.
  void NotifyWindowCloseButtonClicked();
  void NotifyWindowClosed();
  void NotifyWindowEndSession();
  void NotifyWindowBlur();
  void NotifyWindowFocus();
  void NotifyWindowShow();
  void NotifyWindowIsKeyChanged(bool is_key);
  void NotifyWindowHide();
  void NotifyWindowMaximize();
  void NotifyWindowUnmaximize();
  void NotifyWindowMinimize();
  void NotifyWindowRestore();
  void NotifyWindowMove();
  void NotifyWindowWillResize(const gfx::Rect& new_bounds,
                              const gfx::ResizeEdge& edge,
                              bool& prevent_default);
  void NotifyWindowResize();
  void NotifyWindowResized();
  void NotifyWindowWillMove(const gfx::Rect& new_bounds, bool& prevent_default);
  void NotifyWindowMoved();
  void NotifyWindowScrollTouchBegin();
  void NotifyWindowScrollTouchEnd();
  void NotifyWindowSwipe(const std::string& direction);
  void NotifyWindowRotateGesture(float rotation);
  void NotifyWindowSheetBegin();
  void NotifyWindowSheetEnd();
  virtual void NotifyWindowEnterFullScreen();
  virtual void NotifyWindowLeaveFullScreen();
  void NotifyWindowAlwaysOnTopChanged();
  void NotifyWindowExecuteAppCommand(const std::string& command);
  void NotifyTouchBarItemInteraction(const std::string& item_id,
                                     const base::Value::Dict& details);
  void NotifyWindowSystemContextMenu(int x, int y, bool& prevent_default);

#if BUILDFLAG(IS_WIN)
  void NotifyWindowMessage(UINT message, WPARAM w_param, LPARAM l_param);
#endif

  void AddObserver(NativeWindowObserver* obs) { observers_.AddObserver(obs); }
  void RemoveObserver(NativeWindowObserver* obs) {
    observers_.RemoveObserver(obs);
  }

  enum class TitleBarStyle {
    kNormal,
    kHidden,
    kHiddenInset,
    kCustomButtonsOnHover,
  };
  TitleBarStyle title_bar_style() const { return title_bar_style_; }

  bool has_frame() const { return has_frame_; }
  void set_has_frame(bool has_frame) { has_frame_ = has_frame; }
  bool transparent() const { return transparent_; }

  NativeWindow* parent() const { return parent_; }
  bool is_modal() const { return is_modal_; }
  int32_t window_id() const { return next_id_; }

 protected:
  NativeWindow(const gin_helper::Dictionary& options, NativeWindow* parent);

  // Converts between content bounds and window bounds.
  virtual gfx::Rect ContentBoundsToWindowBounds(
      const gfx::Rect& bounds) const = 0;
  virtual gfx::Rect WindowBoundsToContentBounds(
      const gfx::Rect& bounds) const = 0;

  // The boolean parsing of the "titleBarOverlay" option
  bool titlebar_overlay_ = false;

  // The "titleBarStyle" option.
  TitleBarStyle title_bar_style_ = TitleBarStyle::kNormal;

  // Minimum and maximum size.
  std::optional<SizeConstraints> size_constraints_;

 private:
  static int32_t next_id_;

  // Whether window has standard frame.
  bool has_frame_ = true;

  // Whether window is transparent.
  bool transparent_ = false;

  // The windows has been closed.
  bool is_closed_ = false;

  // Used to display sheets at the appropriate horizontal and vertical offsets
  // on macOS.
  double sheet_offset_x_ = 0.0;
  double sheet_offset_y_ = 0.0;

  // Used to maintain the aspect ratio of a view which is inside of the
  // content view.
  double aspect_ratio_ = 0.0;
  gfx::Size aspect_ratio_extraSize_;

  // The parent window, it is guaranteed to be valid during this window's life.
  raw_ptr<NativeWindow> parent_ = nullptr;

  // Is this a modal window.
  bool is_modal_ = false;

  std::string vibrancy_;
  // Observers of this window.
  base::ObserverList<NativeWindowObserver> observers_;

  // Accessible title.
  std::u16string accessible_title_;

  base::WeakPtrFactory<NativeWindow> weak_factory_{this};
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_NATIVE_WINDOW_H_
