// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/app/native_window.h"

#include <algorithm>
#include <string>
#include <vector>

// #include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "shell/app/application.h"
#include "shell/app/window_list.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/persistent_dictionary.h"
#include "shell/common/options_switches.h"

#if BUILDFLAG(IS_WIN)
#include "shell/api/dpi_win.h"
#endif

namespace gin {

template <>
struct Converter<lynxtron::NativeWindow::TitleBarStyle> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     lynxtron::NativeWindow::TitleBarStyle* out) {
    using TitleBarStyle = lynxtron::NativeWindow::TitleBarStyle;
    std::string title_bar_style;
    if (!ConvertFromV8(isolate, val, &title_bar_style)) {
      return false;
    }
    if (title_bar_style == "hidden") {
      *out = TitleBarStyle::kHidden;
#if BUILDFLAG(IS_MAC)
    } else if (title_bar_style == "hiddenInset") {
      *out = TitleBarStyle::kHiddenInset;
    } else if (title_bar_style == "customButtonsOnHover") {
      *out = TitleBarStyle::kCustomButtonsOnHover;
#endif
    } else {
      return false;
    }
    return true;
  }
};

}  // namespace gin

namespace lynxtron {

namespace {
#if BUILDFLAG(IS_WIN)
gfx::Size GetExpandedWindowSize(const NativeWindow* window,
                                bool transparent,
                                gfx::Size size) {
  gfx::Size min_size =
      ScreenToDIPSize(window->GetNativeWindowHandle(), gfx::Size{64, 64});

  // Some AMD drivers can't display windows that are less than 64x64 pixels,
  // so expand them to be at least that size. http://crbug.com/286609
  gfx::Size expanded(std::max(size.width(), min_size.width()),
                     std::max(size.height(), min_size.height()));
  return expanded;
}
#endif

}  // namespace
NativeWindow::NativeWindow(const gin_helper::Dictionary& options,
                           NativeWindow* parent)
    : parent_(parent) {
  ++next_id_;

  options.Get(options::kFrame, &has_frame_);
  options.Get(options::kTransparent, &transparent_);
  options.Get(options::kEnableLargerThanScreen, &enable_larger_than_screen_);
  options.Get(options::kTitleBarStyle, &title_bar_style_);

  if (parent) {
    options.Get("modal", &is_modal_);
  }

  WindowList::AddWindow(this);
}

NativeWindow::~NativeWindow() {
  NotifyWindowClosed();
}

void NativeWindow::InitFromOptions(const gin_helper::Dictionary& options) {
  // Setup window from options.
  if (int x, y; options.Get(options::kX, &x) && options.Get(options::kY, &y)) {
    SetPosition(gfx::Point{x, y}, false);

#if BUILDFLAG(IS_WIN)
    // FIXME(felixrieseberg): Dirty, dirty workaround for
    // https://github.com/electron/electron/issues/10862
    // Somehow, we need to call `SetBounds` twice to get
    // usable results. The root cause is still unknown.
    SetPosition(gfx::Point{x, y}, false);
#endif
  } else if (bool center; options.Get(options::kCenter, &center) && center) {
    Center();
  }

  const bool use_content_size =
      options.ValueOrDefault(options::kUseContentSize, false);

  // On Linux and Window we may already have maximum size defined.
  SizeConstraints size_constraints(
      use_content_size ? GetContentSizeConstraints() : GetSizeConstraints());

  const int min_width = options.ValueOrDefault(
      options::kMinWidth, size_constraints.GetMinimumSize().width());
  const int min_height = options.ValueOrDefault(
      options::kMinHeight, size_constraints.GetMinimumSize().height());
  size_constraints.set_minimum_size(gfx::Size(min_width, min_height));

  gfx::Size max_size = size_constraints.GetMaximumSize();
  int max_width = max_size.width() > 0 ? max_size.width() : INT_MAX;
  int max_height = max_size.height() > 0 ? max_size.height() : INT_MAX;
  bool have_max_width = options.Get(options::kMaxWidth, &max_width);
  if (have_max_width && max_width <= 0) {
    max_width = INT_MAX;
  }
  bool have_max_height = options.Get(options::kMaxHeight, &max_height);
  if (have_max_height && max_height <= 0) {
    max_height = INT_MAX;
  }

  // By default the window has a default maximum size that prevents it
  // from being resized larger than the screen, so we should only set this
  // if the user has passed in values.
  if (have_max_height || have_max_width || !max_size.IsEmpty()) {
    size_constraints.set_maximum_size(gfx::Size(max_width, max_height));
  }

  if (use_content_size) {
    SetContentSizeConstraints(size_constraints);
  } else {
    SetSizeConstraints(size_constraints);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  if (bool val; options.Get(options::kClosable, &val)) {
    SetClosable(val);
  }
#endif

  if (bool val; options.Get(options::kMovable, &val)) {
    SetMovable(val);
  }

  if (bool val; options.Get(options::kHasShadow, &val)) {
    SetHasShadow(val);
  }

  if (double val; options.Get(options::kOpacity, &val)) {
    SetOpacity(val);
  }

  if (bool val; options.Get(options::kAlwaysOnTop, &val) && val) {
    SetAlwaysOnTop(ui::ZOrderLevel::kFloatingWindow);
  }

  bool fullscreenable = true;
  bool fullscreen = false;
  if (options.Get(options::kFullscreen, &fullscreen) && !fullscreen) {
    // Disable fullscreen button if 'fullscreen' is specified to false.
#if BUILDFLAG(IS_MAC)
    fullscreenable = false;
#endif
  }

  options.Get(options::kFullScreenable, &fullscreenable);
  SetFullScreenable(fullscreenable);

  if (fullscreen) {
    SetFullScreen(true);
  }

  if (bool val; options.Get(options::kResizable, &val)) {
    SetResizable(val);
  }

  if (bool val; options.Get(options::kSkipTaskbar, &val)) {
    SetSkipTaskbar(val);
  }

  if (bool val; options.Get(options::kKiosk, &val) && val) {
    SetKiosk(val);
  }

#if BUILDFLAG(IS_MAC)
  if (std::string val; options.Get(options::kVibrancyType, &val)) {
    SetVibrancy(val, 0);
  }
#endif

  // TODO(Guo Xi): SetBackgroundMaterial
  // #elif BUILDFLAG(IS_WIN)
  //   if (std::string val; options.Get(options::kBackgroundMaterial, &val))
  //     SetBackgroundMaterial(val);
  // #endif
#

  // TODO(Guo Xi): refer DesktopWindowTreeHostWin::SetBackgroundColor
  // SkColor background_color = SK_ColorWHITE;
  // if (std::string color; options.Get(options::kBackgroundColor, &color)) {
  //   background_color = ParseCSSColor(color).value_or(SK_ColorWHITE);
  // } else if (IsTranslucent()) {
  //   background_color = SK_ColorTRANSPARENT;
  // }
  // SetBackgroundColor(background_color);

  SetTitle(
      options.ValueOrDefault(options::kTitle, Application::Get()->GetName()));

  // Then show it.
  if (options.ValueOrDefault(options::kShow, true)) {
    Show();
  }
}

// TODO(Guo Xi): support SetShape
void NativeWindow::SetShape(const std::vector<gfx::Rect>& rects) {
  //  widget()->SetShape(std::make_unique<std::vector<gfx::Rect>>(rects));
}

bool NativeWindow::IsClosed() const {
  return is_closed_;
}

void NativeWindow::SetSize(const gfx::Size& size, bool animate) {
  SetBounds(gfx::Rect(GetPosition(), size), animate);
}

gfx::Size NativeWindow::GetSize() const {
  return GetBounds().size();
}

float NativeWindow::GetDevicePixelRatio() const {
  return 1.0f;
}

void NativeWindow::SetPosition(const gfx::Point& position, bool animate) {
  SetBounds(gfx::Rect(position, GetSize()), animate);
}

gfx::Point NativeWindow::GetPosition() const {
  return GetBounds().origin();
}

void NativeWindow::SetContentSize(const gfx::Size& size, bool animate) {
  SetSize(ContentBoundsToWindowBounds(gfx::Rect(size)).size(), animate);
}

gfx::Size NativeWindow::GetContentSize() const {
  return GetContentBounds().size();
}

void NativeWindow::SetContentBounds(const gfx::Rect& bounds, bool animate) {
  SetBounds(ContentBoundsToWindowBounds(bounds), animate);
}

gfx::Rect NativeWindow::GetContentBounds() const {
  return WindowBoundsToContentBounds(GetBounds());
}

bool NativeWindow::IsNormal() const {
  return !IsMinimized() && !IsMaximized() && !IsFullscreen();
}

void NativeWindow::SetSizeConstraints(
    const SizeConstraints& window_constraints) {
  size_constraints_ = window_constraints;
  content_size_constraints_.reset();
}

SizeConstraints NativeWindow::GetSizeConstraints() const {
  if (size_constraints_) {
    return *size_constraints_;
  }
  if (!content_size_constraints_) {
    return {};
  }
  // Convert content size constraints to window size constraints.
  SizeConstraints constraints;
  if (content_size_constraints_->HasMaximumSize()) {
    gfx::Rect max_bounds = ContentBoundsToWindowBounds(
        gfx::Rect(content_size_constraints_->GetMaximumSize()));
    constraints.set_maximum_size(max_bounds.size());
  }
  if (content_size_constraints_->HasMinimumSize()) {
    gfx::Rect min_bounds = ContentBoundsToWindowBounds(
        gfx::Rect(content_size_constraints_->GetMinimumSize()));
    constraints.set_minimum_size(min_bounds.size());
  }
  return constraints;
}

void NativeWindow::SetContentSizeConstraints(
    const SizeConstraints& size_constraints) {
  content_size_constraints_ = size_constraints;
  size_constraints_.reset();
}

SizeConstraints NativeWindow::GetContentSizeConstraints() const {
  if (content_size_constraints_) {
    return *content_size_constraints_;
  }
  if (!size_constraints_) {
    return {};
  }
  // Convert window size constraints to content size constraints.
  // Note that we are not caching the results, because Chromium reccalculates
  // window frame size everytime when min/max sizes are passed, and we must
  // do the same otherwise the resulting size with frame included will be wrong.
  SizeConstraints constraints;
  if (size_constraints_->HasMaximumSize()) {
    gfx::Rect max_bounds = WindowBoundsToContentBounds(
        gfx::Rect(size_constraints_->GetMaximumSize()));
    constraints.set_maximum_size(max_bounds.size());
  }
  if (size_constraints_->HasMinimumSize()) {
    gfx::Rect min_bounds = WindowBoundsToContentBounds(
        gfx::Rect(size_constraints_->GetMinimumSize()));
    constraints.set_minimum_size(min_bounds.size());
  }
  return constraints;
}

void NativeWindow::SetMinimumSize(const gfx::Size& size) {
  SizeConstraints size_constraints = GetSizeConstraints();
  size_constraints.set_minimum_size(size);
  SetSizeConstraints(size_constraints);
}

gfx::Size NativeWindow::GetMinimumSize() const {
  return GetSizeConstraints().GetMinimumSize();
}

void NativeWindow::SetMaximumSize(const gfx::Size& size) {
  SizeConstraints size_constraints = GetSizeConstraints();
  size_constraints.set_maximum_size(size);
  SetSizeConstraints(size_constraints);
}

gfx::Size NativeWindow::GetMaximumSize() const {
  return GetSizeConstraints().GetMaximumSize();
}

gfx::Size NativeWindow::GetContentMinimumSize() const {
  return GetContentSizeConstraints().GetMinimumSize();
}

gfx::Size NativeWindow::GetContentMaximumSize() const {
  const auto size_constraints = GetContentSizeConstraints();
  gfx::Size maximum_size = size_constraints.GetMaximumSize();

#if BUILDFLAG(IS_WIN)
  if (size_constraints.HasMaximumSize()) {
    maximum_size = GetExpandedWindowSize(this, transparent(), maximum_size);
  }
#endif

  return maximum_size;
}

// TODO(Guo Xi): 在 macOS 上review sheet 相关的逻辑
void NativeWindow::SetSheetOffset(const double offsetX, const double offsetY) {
  sheet_offset_x_ = offsetX;
  sheet_offset_y_ = offsetY;
}

double NativeWindow::GetSheetOffsetX() {
  return sheet_offset_x_;
}

double NativeWindow::GetSheetOffsetY() {
  return sheet_offset_y_;
}

bool NativeWindow::IsTabletMode() const {
  return false;
}

void NativeWindow::SetRepresentedFilename(const std::string& filename) {}

std::string NativeWindow::GetRepresentedFilename() {
  return "";
}

void NativeWindow::SetFocusable(bool focusable) {}

bool NativeWindow::IsFocusable() const {
  return false;
}

void NativeWindow::SetParentWindow(NativeWindow* parent) {
  parent_ = parent;
}

void NativeWindow::SetAutoHideCursor(bool auto_hide) {}

void NativeWindow::SetVibrancy(const std::string& type, int duration) {
  vibrancy_ = type;
}

void NativeWindow::SetTouchBar(
    std::vector<gin_helper::PersistentDictionary> items) {}

void NativeWindow::RefreshTouchBarItem(const std::string& item_id) {}

void NativeWindow::SetEscapeTouchBarItem(
    gin_helper::PersistentDictionary item) {}

void NativeWindow::SetAutoHideMenuBar(bool auto_hide) {}

bool NativeWindow::IsMenuBarAutoHide() {
  return false;
}

bool NativeWindow::IsMenuBarVisible() {
  return true;
}

double NativeWindow::GetAspectRatio() const {
  return aspect_ratio_;
}

gfx::Size NativeWindow::GetAspectRatioExtraSize() {
  return aspect_ratio_extraSize_;
}

void NativeWindow::SetAspectRatio(double aspect_ratio,
                                  const gfx::Size& extra_size) {
  aspect_ratio_ = aspect_ratio;
  aspect_ratio_extraSize_ = extra_size;
}

// void NativeWindow::PreviewFile(const std::string& path,
//                                const std::string& display_name) {}

// void NativeWindow::CloseFilePreview() {}

void NativeWindow::NotifyWindowRequestPreferredWidth(int* width) {
  observers_.Notify(&NativeWindowObserver::RequestPreferredWidth, width);
}

void NativeWindow::NotifyWindowCloseButtonClicked() {
  // First ask the observers whether we want to close.
  bool prevent_default = false;
  observers_.Notify(&NativeWindowObserver::WillCloseWindow,
                    std::ref(prevent_default));
  if (prevent_default) {
    WindowList::WindowCloseCancelled(this);
    return;
  }

  // Then ask the observers how should we close the window.
  prevent_default = false;
  observers_.Notify(&NativeWindowObserver::OnCloseButtonClicked,
                    std::ref(prevent_default));
  if (prevent_default) {
    return;
  }

  CloseImmediately();
}

void NativeWindow::NotifyWindowClosed() {
  if (is_closed_) {
    return;
  }

  is_closed_ = true;
  observers_.Notify(&NativeWindowObserver::OnWindowClosed);
  WindowList::RemoveWindow(this);
}

void NativeWindow::NotifyWindowEndSession() {
  observers_.Notify(&NativeWindowObserver::OnWindowEndSession);
}

void NativeWindow::NotifyWindowBlur() {
  observers_.Notify(&NativeWindowObserver::OnWindowBlur);
}

void NativeWindow::NotifyWindowFocus() {
  observers_.Notify(&NativeWindowObserver::OnWindowFocus);
}

void NativeWindow::NotifyWindowIsKeyChanged(bool is_key) {
  observers_.Notify(&NativeWindowObserver::OnWindowIsKeyChanged, is_key);
}

void NativeWindow::NotifyWindowShow() {
  observers_.Notify(&NativeWindowObserver::OnWindowShow);
}

void NativeWindow::NotifyWindowHide() {
  observers_.Notify(&NativeWindowObserver::OnWindowHide);
}

void NativeWindow::NotifyWindowMaximize() {
  observers_.Notify(&NativeWindowObserver::OnWindowMaximize);
}

void NativeWindow::NotifyWindowUnmaximize() {
  observers_.Notify(&NativeWindowObserver::OnWindowUnmaximize);
}

void NativeWindow::NotifyWindowMinimize() {
  observers_.Notify(&NativeWindowObserver::OnWindowMinimize);
}

void NativeWindow::NotifyWindowRestore() {
  observers_.Notify(&NativeWindowObserver::OnWindowRestore);
}

void NativeWindow::NotifyWindowWillResize(const gfx::Rect& new_bounds,
                                          const gfx::ResizeEdge& edge,
                                          bool& prevent_default) {
  observers_.Notify(&NativeWindowObserver::OnWindowWillResize, new_bounds, edge,
                    std::ref(prevent_default));
}

void NativeWindow::NotifyWindowWillMove(const gfx::Rect& new_bounds,
                                        bool& prevent_default) {
  observers_.Notify(&NativeWindowObserver::OnWindowWillMove, new_bounds,
                    std::ref(prevent_default));
}

void NativeWindow::NotifyWindowResize() {
  observers_.Notify(&NativeWindowObserver::OnWindowResize);
}

void NativeWindow::NotifyWindowResized() {
  observers_.Notify(&NativeWindowObserver::OnWindowResized);
}

void NativeWindow::NotifyWindowMove() {
  observers_.Notify(&NativeWindowObserver::OnWindowMove);
}

void NativeWindow::NotifyWindowMoved() {
  observers_.Notify(&NativeWindowObserver::OnWindowMoved);
}

void NativeWindow::NotifyWindowEnterFullScreen() {
  observers_.Notify(&NativeWindowObserver::OnWindowEnterFullScreen);
}

void NativeWindow::NotifyWindowScrollTouchBegin() {
  observers_.Notify(&NativeWindowObserver::OnWindowScrollTouchBegin);
}

void NativeWindow::NotifyWindowScrollTouchEnd() {
  observers_.Notify(&NativeWindowObserver::OnWindowScrollTouchEnd);
}

void NativeWindow::NotifyWindowSwipe(const std::string& direction) {
  observers_.Notify(&NativeWindowObserver::OnWindowSwipe, direction);
}

void NativeWindow::NotifyWindowRotateGesture(float rotation) {
  observers_.Notify(&NativeWindowObserver::OnWindowRotateGesture, rotation);
}

void NativeWindow::NotifyWindowSheetBegin() {
  observers_.Notify(&NativeWindowObserver::OnWindowSheetBegin);
}

void NativeWindow::NotifyWindowSheetEnd() {
  observers_.Notify(&NativeWindowObserver::OnWindowSheetEnd);
}

void NativeWindow::NotifyWindowLeaveFullScreen() {
  observers_.Notify(&NativeWindowObserver::OnWindowLeaveFullScreen);
}

void NativeWindow::NotifyWindowAlwaysOnTopChanged() {
  observers_.Notify(&NativeWindowObserver::OnWindowAlwaysOnTopChanged);
}

void NativeWindow::NotifyWindowExecuteAppCommand(const std::string& command) {
  observers_.Notify(&NativeWindowObserver::OnExecuteAppCommand, command);
}

void NativeWindow::NotifyTouchBarItemInteraction(
    const std::string& item_id,
    const base::Value::Dict& details) {
  observers_.Notify(&NativeWindowObserver::OnTouchBarItemResult, item_id,
                    details);
}

void NativeWindow::NotifyWindowSystemContextMenu(int x,
                                                 int y,
                                                 bool& prevent_default) {
  observers_.Notify(&NativeWindowObserver::OnSystemContextMenu, x, y,
                    std::ref(prevent_default));
}

// void NativeWindow::NotifyLayoutWindowControlsOverlay() {
//   gfx::Rect bounding_rect = GetWindowControlsOverlayRect();
//   if (!bounding_rect.IsEmpty()) {
//     observers_.Notify(&NativeWindowObserver::UpdateWindowControlsOverlay,
//                       bounding_rect);
//   }
// }

#if BUILDFLAG(IS_WIN)
void NativeWindow::NotifyWindowMessage(UINT message,
                                       WPARAM w_param,
                                       LPARAM l_param) {
  observers_.Notify(&NativeWindowObserver::OnWindowMessage, message, w_param,
                    l_param);
}
#endif

void NativeWindow::SetAccessibleTitle(const std::string& title) {
  accessible_title_ = base::UTF8ToUTF16(title);
}

std::string NativeWindow::GetAccessibleTitle() {
  return base::UTF16ToUTF8(accessible_title_);
}

int32_t NativeWindow::next_id_ = 0;

}  // namespace lynxtron
