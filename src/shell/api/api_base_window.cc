// Copyright (c) 2018 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_base_window.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "gin/dictionary.h"
#include "shell/api/api_native_image.h"
#include "shell/app/javascript_environment.h"
#include "shell/common/color_parser.h"
#include "shell/common/gin_converters/callback_converter.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "shell/common/gin_converters/gfx_converter.h"
#include "shell/common/gin_converters/value_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/handle.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/gin_helper/persistent_dictionary.h"
#include "shell/common/node_includes.h"
#include "shell/common/node_util.h"
#include "shell/common/options_switches.h"

namespace lynxtron::api {

namespace {

// Converts binary data to Buffer.
v8::Local<v8::Value> ToBuffer(v8::Isolate* isolate,
                              const base::span<const uint8_t> val) {
  auto buffer = lynxtron::Buffer::Copy(isolate, val);
  if (buffer.IsEmpty()) {
    return v8::Null(isolate);
  } else {
    return buffer.ToLocalChecked();
  }
}

[[nodiscard]] constexpr std::array<int, 2U> ToArray(const gfx::Size size) {
  return {size.width(), size.height()};
}

[[nodiscard]] constexpr std::array<int, 2U> ToArray(const gfx::Point point) {
  return {point.x(), point.y()};
}

}  // namespace

BaseWindow::BaseWindow(v8::Isolate* isolate,
                       const gin_helper::Dictionary& options) {
  // The parent window.
  gin_helper::Handle<BaseWindow> parent;
  if (options.Get("parent", &parent) && !parent.IsEmpty()) {
    parent_window_.Reset(isolate, parent.ToV8());
  }

  // Creates NativeWindow.
  window_.reset(NativeWindow::Create(
      options, parent.IsEmpty() ? nullptr : parent->window_.get()));

  window_->AddObserver(this);

  v8::Local<v8::Value> icon;
  if (options.Get(options::kIcon, &icon)) {
    SetIcon(isolate, icon);
  }
}

BaseWindow::BaseWindow(gin_helper::Arguments* args,
                       const gin_helper::Dictionary& options)
    : BaseWindow(args->isolate(), options) {
  InitWithArgs(args);
  // Init window after everything has been setup.
  window()->InitFromOptions(options);
}

BaseWindow::~BaseWindow() {
  CloseImmediately();

  // Destroy the native window in next tick because the native code might be
  // iterating all windows.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, window_.release());

  // Remove global reference so the JS object can be garbage collected.
  self_ref_.Reset();
}

void BaseWindow::InitWith(v8::Isolate* isolate, v8::Local<v8::Object> wrapper) {
  AttachAsUserData(window_.get());
  gin_helper::TrackableObject<BaseWindow>::InitWith(isolate, wrapper);

  // We can only append this window to parent window's child windows after this
  // window's JS wrapper gets initialized.
  if (!parent_window_.IsEmpty()) {
    gin_helper::Handle<BaseWindow> parent;
    gin::ConvertFromV8(isolate, GetParentWindow(), &parent);
    DCHECK(!parent.IsEmpty());
    parent->child_windows_.Set(isolate, weak_map_id(), wrapper);
  }

  // Reference this object in case it got garbage collected.
  self_ref_.Reset(isolate, wrapper);
}

void BaseWindow::WillCloseWindow(bool& prevent_default) {
  if (Emit("close")) {
    prevent_default = true;
  }
}

void BaseWindow::OnWindowClosed() {
  // Invalidate weak ptrs before the Javascript object is destroyed,
  // there might be some delayed emit events which shouldn't be
  // triggered after this.
  weak_factory_.InvalidateWeakPtrs();

  RemoveFromWeakMap();
  window_->RemoveObserver(this);

  // We can not call Destroy here because we need to call Emit first, but we
  // also do not want any method to be used, so just mark as destroyed here.
  MarkDestroyed();

  Emit("closed");

  RemoveFromParentChildWindows();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, GetDestroyClosure());
}

// TODO(Guo Xi): Add OnWindowQueryEndSession OnWindowEndSession
void BaseWindow::OnWindowEndSession() {
  Emit("session-end");
}

void BaseWindow::OnWindowBlur() {
  EmitEventSoon("blur");
}

void BaseWindow::OnWindowFocus() {
  EmitEventSoon("focus");
}

void BaseWindow::OnWindowShow() {
  Emit("show");
}

void BaseWindow::OnWindowHide() {
  Emit("hide");
}

void BaseWindow::OnWindowMaximize() {
  Emit("maximize");
}

void BaseWindow::OnWindowUnmaximize() {
  Emit("unmaximize");
}

void BaseWindow::OnWindowMinimize() {
  Emit("minimize");
}

void BaseWindow::OnWindowRestore() {
  Emit("restore");
}

void BaseWindow::OnWindowWillResize(const gfx::Rect& new_bounds,
                                    const gfx::ResizeEdge edge,
                                    bool& prevent_default) {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope handle_scope(isolate);
  auto info = gin::Dictionary::CreateEmpty(isolate);
  info.Set("edge", edge);

  if (Emit("will-resize", new_bounds, info)) {
    prevent_default = true;
  }
}

void BaseWindow::OnWindowResize() {
  Emit("resize");
}

void BaseWindow::OnWindowResized() {
  Emit("resized");
}

void BaseWindow::OnWindowWillMove(const gfx::Rect& new_bounds,
                                  bool& prevent_default) {
  if (Emit("will-move", new_bounds)) {
    prevent_default = true;
  }
}

void BaseWindow::OnWindowMove() {
  Emit("move");
}

void BaseWindow::OnWindowMoved() {
  Emit("moved");
}

void BaseWindow::OnWindowWillEnterFullScreen() {
  Emit("will-enter-full-screen");
}

void BaseWindow::OnWindowWillLeaveFullScreen() {
  Emit("will-leave-full-screen");
}

void BaseWindow::OnWindowEnterFullScreen() {
  Emit("enter-full-screen");
}

void BaseWindow::OnWindowLeaveFullScreen() {
  Emit("leave-full-screen");
}

void BaseWindow::OnWindowScrollTouchBegin() {
  Emit("scroll-touch-begin");
}

void BaseWindow::OnWindowScrollTouchEnd() {
  Emit("scroll-touch-end");
}

void BaseWindow::OnWindowSwipe(const std::string& direction) {
  Emit("swipe", direction);
}

void BaseWindow::OnWindowRotateGesture(float rotation) {
  Emit("rotate-gesture", rotation);
}

void BaseWindow::OnWindowSheetBegin() {
  Emit("sheet-begin");
}

void BaseWindow::OnWindowSheetEnd() {
  Emit("sheet-end");
}

void BaseWindow::OnWindowAlwaysOnTopChanged() {
  Emit("always-on-top-changed", IsAlwaysOnTop());
}

void BaseWindow::OnExecuteAppCommand(const std::string_view command_name) {
  Emit("app-command", command_name);
}

void BaseWindow::OnTouchBarItemResult(const std::string& item_id,
                                      const base::Value::Dict& details) {
  Emit("-touch-bar-interaction", item_id, details);
}

void BaseWindow::OnNewWindowForTab() {
  Emit("new-window-for-tab");
}

void BaseWindow::OnSystemContextMenu(int x, int y, bool& prevent_default) {
  if (Emit("system-context-menu", gfx::Point(x, y))) {
    prevent_default = true;
  }
}

#if BUILDFLAG(IS_WIN)
void BaseWindow::OnWindowMessage(UINT message, WPARAM w_param, LPARAM l_param) {
  if (IsWindowMessageHooked(message)) {
    v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
    v8::HandleScope scope(isolate);
    messages_callback_map_[message].Run(
        ToBuffer(isolate, base::byte_span_from_ref(w_param)),
        ToBuffer(isolate, base::byte_span_from_ref(l_param)));
  }
}
#endif

void BaseWindow::CloseImmediately() {
  if (!window_->IsClosed()) {
    window_->CloseImmediately();
  }
}

void BaseWindow::Close() {
  window_->Close();
}

void BaseWindow::Focus() {
  window_->Focus(true);
}

void BaseWindow::Blur() {
  window_->Focus(false);
}

bool BaseWindow::IsFocused() const {
  return window_->IsFocused();
}

void BaseWindow::Show() {
  window_->Show();
}

void BaseWindow::ShowInactive() {
  // This method doesn't make sense for modal window.
  if (IsModal()) {
    return;
  }
  window_->ShowInactive();
}

void BaseWindow::Hide() {
  window_->Hide();
}

bool BaseWindow::IsVisible() const {
  return window_->IsVisible();
}

bool BaseWindow::IsEnabled() const {
  return window_->IsEnabled();
}

void BaseWindow::SetEnabled(bool enable) {
  window_->SetEnabled(enable);
}

void BaseWindow::Maximize() {
  window_->Maximize();
}

void BaseWindow::Unmaximize() {
  window_->Unmaximize();
}

bool BaseWindow::IsMaximized() const {
  return window_->IsMaximized();
}

void BaseWindow::Minimize() {
  window_->Minimize();
}

void BaseWindow::Restore() {
  window_->Restore();
}

bool BaseWindow::IsMinimized() const {
  return window_->IsMinimized();
}

void BaseWindow::SetFullScreen(bool fullscreen) {
  window_->SetFullScreen(fullscreen);
}

bool BaseWindow::IsFullscreen() const {
  return window_->IsFullscreen();
}

void BaseWindow::SetBounds(const gfx::Rect& bounds,
                           gin_helper::Arguments* args) {
  bool animate = false;
  args->GetNext(&animate);
  window_->SetBounds(bounds, animate);
}

gfx::Rect BaseWindow::GetBounds() const {
  return window_->GetBounds();
}

bool BaseWindow::IsNormal() const {
  return window_->IsNormal();
}

gfx::Rect BaseWindow::GetNormalBounds() const {
  return window_->GetNormalBounds();
}

// void BaseWindow::SetContentBounds(const gfx::Rect& bounds,
//                                   gin_helper::Arguments* args) {
//   bool animate = false;
//   args->GetNext(&animate);
//   window_->SetContentBounds(bounds, animate);
// }

// gfx::Rect BaseWindow::GetContentBounds() const {
//   return window_->GetContentBounds();
// }

void BaseWindow::SetContentBounds(const gfx::Rect& bounds,
                                  gin_helper::Arguments* args) {
  bool animate = false;
  args->GetNext(&animate);
  window_->SetContentBounds(bounds, animate);
}

gfx::Rect BaseWindow::GetContentBounds() const {
  return window_->GetContentBounds();
}

void BaseWindow::SetSize(int width, int height, gin_helper::Arguments* args) {
  bool animate = false;
  gfx::Size size = window_->GetMinimumSize();
  size.SetToMax(gfx::Size(width, height));
  args->GetNext(&animate);
  window_->SetSize(size, animate);
}

std::array<int, 2U> BaseWindow::GetSize() const {
  return ToArray(window_->GetSize());
}

// void BaseWindow::SetContentSize(int width,
//                                     int height,
//                                     gin_helper::Arguments* args) {
//   bool animate = false;
//   args->GetNext(&animate);
//   window_->SetContentSize(gfx::Size(width, height), animate);
// }

// std::vector<int> BaseWindow::GetContentSize() {
//   std::vector<int> result(2);
//   gfx::Size size = window_->GetContentSize();
//   result[0] = size.width();
//   result[1] = size.height();
//   return result;
// }

void BaseWindow::SetContentSize(int width,
                                int height,
                                gin_helper::Arguments* args) {
  bool animate = false;
  args->GetNext(&animate);
  window_->SetContentSize(gfx::Size(width, height), animate);
}

std::array<int, 2U> BaseWindow::GetContentSize() const {
  return ToArray(window_->GetContentSize());
}

void BaseWindow::SetMinimumSize(int width, int height) {
  window_->SetMinimumSize(gfx::Size(width, height));
}

std::array<int, 2U> BaseWindow::GetMinimumSize() const {
  return ToArray(window_->GetMinimumSize());
}

void BaseWindow::SetMaximumSize(int width, int height) {
  window_->SetMaximumSize(gfx::Size(width, height));
}

std::array<int, 2U> BaseWindow::GetMaximumSize() const {
  return ToArray(window_->GetMaximumSize());
}

void BaseWindow::SetSheetOffset(double offsetY, gin_helper::Arguments* args) {
  double offsetX = 0.0;
  args->GetNext(&offsetX);
  window_->SetSheetOffset(offsetX, offsetY);
}

void BaseWindow::SetResizable(bool resizable) {
  window_->SetResizable(resizable);
}

bool BaseWindow::IsResizable() const {
  return window_->IsResizable();
}

void BaseWindow::SetMovable(bool movable) {
  window_->SetMovable(movable);
}

bool BaseWindow::IsMovable() const {
  return window_->IsMovable();
}

void BaseWindow::SetMinimizable(bool minimizable) {
  window_->SetMinimizable(minimizable);
}

bool BaseWindow::IsMinimizable() const {
  return window_->IsMinimizable();
}

void BaseWindow::SetMaximizable(bool maximizable) {
  window_->SetMaximizable(maximizable);
}

bool BaseWindow::IsMaximizable() const {
  return window_->IsMaximizable();
}

void BaseWindow::SetFullScreenable(bool fullscreenable) {
  window_->SetFullScreenable(fullscreenable);
}

bool BaseWindow::IsFullScreenable() const {
  return window_->IsFullScreenable();
}

void BaseWindow::SetClosable(bool closable) {
  window_->SetClosable(closable);
}

bool BaseWindow::IsClosable() const {
  return window_->IsClosable();
}

void BaseWindow::SetAlwaysOnTop(bool top, gin_helper::Arguments* args) {
  std::string level = "floating";
  int relative_level = 0;
  args->GetNext(&level);
  args->GetNext(&relative_level);

  ui::ZOrderLevel z_order =
      top ? ui::ZOrderLevel::kFloatingWindow : ui::ZOrderLevel::kNormal;
  window_->SetAlwaysOnTop(z_order, level, relative_level);
}

bool BaseWindow::IsAlwaysOnTop() const {
  return window_->GetZOrderLevel() != ui::ZOrderLevel::kNormal;
}

void BaseWindow::Center() {
  window_->Center();
}

void BaseWindow::SetPosition(int x, int y, gin_helper::Arguments* args) {
  bool animate = false;
  args->GetNext(&animate);
  window_->SetPosition(gfx::Point(x, y), animate);
}

std::array<int, 2U> BaseWindow::GetPosition() const {
  return ToArray(window_->GetPosition());
}

void BaseWindow::MoveTop() {
  window_->MoveTop();
}

void BaseWindow::SetTitle(const std::string& title) {
  window_->SetTitle(title);
}

std::string BaseWindow::GetTitle() const {
  return window_->GetTitle();
}

void BaseWindow::SetIcon(v8::Isolate* isolate, v8::Local<v8::Value> icon) {
  if (icon->IsNullOrUndefined()) {
    return;
  }
  api::NativeImage* native_image = nullptr;
  if (!NativeImage::TryConvertNativeImage(isolate, icon, &native_image,
                                          NativeImage::OnConvertError::kWarn)) {
    return;
  }
  if (!native_image || native_image->image().IsEmpty()) {
    return;
  }
  window_->SetIcon(native_image);
}

void BaseWindow::FlashFrame(bool flash) {
  window_->FlashFrame(flash);
}

void BaseWindow::SetSkipTaskbar(bool skip) {
  window_->SetSkipTaskbar(skip);
}

void BaseWindow::SetExcludedFromShownWindowsMenu(bool excluded) {
  window_->SetExcludedFromShownWindowsMenu(excluded);
}

bool BaseWindow::IsExcludedFromShownWindowsMenu() const {
  return window_->IsExcludedFromShownWindowsMenu();
}

void BaseWindow::SetSimpleFullScreen(bool simple_fullscreen) {
  window_->SetSimpleFullScreen(simple_fullscreen);
}

bool BaseWindow::IsSimpleFullScreen() const {
  return window_->IsSimpleFullScreen();
}

void BaseWindow::SetBackgroundColor(const std::string& color_name) {
  SkColor color = window_->GetBackgroundColor();
  if (!content::ParseCssColorString(color_name, &color)) {
    return;
  }

  window_->SetBackgroundColor(color);
}

std::string BaseWindow::GetBackgroundColor() const {
  const SkColor color = window_->GetBackgroundColor();
  return base::StringPrintf("#%02X%02X%02X", SkColorGetR(color),
                            SkColorGetG(color), SkColorGetB(color));
}

void BaseWindow::SetHasShadow(bool has_shadow) {
  window_->SetHasShadow(has_shadow);
}

bool BaseWindow::HasShadow() const {
  return window_->HasShadow();
}

void BaseWindow::SetOpacity(const double opacity) {
  window_->SetOpacity(opacity);
}

double BaseWindow::GetOpacity() const {
  return window_->GetOpacity();
}

void BaseWindow::SetFocusable(bool focusable) {
  return window_->SetFocusable(focusable);
}

bool BaseWindow::IsFocusable() const {
  return window_->IsFocusable();
}

void BaseWindow::SetMenu(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  // auto context = isolate->GetCurrentContext();
  // gin::Handle<Menu> menu;
  // v8::Local<v8::Object> object;
  // if (value->IsObject() && value->ToObject(context).ToLocal(&object) &&
  //    gin::ConvertFromV8(isolate, value, &menu) && !menu.IsEmpty()) {
  //  menu_.Reset(isolate, menu.ToV8());

  //  // We only want to update the menu if the menu has a non-zero item count,
  //  // or we risk crashes.
  //  if (menu->model()->GetItemCount() == 0) {
  //    RemoveMenu();
  //  } else {
  //    window_->SetMenu(menu->model());
  //  }
  //} else if (value->IsNull()) {
  //  RemoveMenu();
  //} else {
  //  isolate->ThrowException(
  //      v8::Exception::TypeError(gin::StringToV8(isolate, "Invalid Menu")));
  //}
}

void BaseWindow::RemoveMenu() {
  // menu_.Reset();
  // window_->SetMenu(nullptr);
}

void BaseWindow::SetParentWindow(v8::Local<v8::Value> value,
                                 gin_helper::Arguments* args) {
  if (IsModal()) {
    args->ThrowError("Can not be called for modal window");
    return;
  }

  gin_helper::Handle<BaseWindow> parent;
  if (value->IsNull() || value->IsUndefined()) {
    RemoveFromParentChildWindows();
    parent_window_.Reset();
    window_->SetParentWindow(nullptr);
  } else if (gin::ConvertFromV8(isolate(), value, &parent)) {
    RemoveFromParentChildWindows();
    parent_window_.Reset(isolate(), value);
    window_->SetParentWindow(parent->window_.get());
    parent->child_windows_.Set(isolate(), weak_map_id(), GetWrapper());
  } else {
    args->ThrowError("Must pass BaseWindow instance or null");
  }
}

v8::Local<v8::Value> BaseWindow::GetNativeWindowHandle() {
  NativeWindowHandle handle = window_->GetNativeWindowHandle();
  return ToBuffer(isolate(), base::byte_span_from_ref(handle));
}

void BaseWindow::SetVisibleOnAllWorkspaces(bool visible,
                                           gin_helper::Arguments* args) {
  gin_helper::Dictionary options;
  bool visibleOnFullScreen = false;
  bool skipTransformProcessType = false;
  if (args->GetNext(&options)) {
    options.Get("visibleOnFullScreen", &visibleOnFullScreen);
    options.Get("skipTransformProcessType", &skipTransformProcessType);
  }
  return window_->SetVisibleOnAllWorkspaces(visible, visibleOnFullScreen,
                                            skipTransformProcessType);
}

bool BaseWindow::IsVisibleOnAllWorkspaces() const {
  return window_->IsVisibleOnAllWorkspaces();
}

void BaseWindow::SetAutoHideCursor(bool auto_hide) {
  window_->SetAutoHideCursor(auto_hide);
}

void BaseWindow::SetAutoHideMenuBar(bool auto_hide) {
  window_->SetAutoHideMenuBar(auto_hide);
}

bool BaseWindow::IsMenuBarAutoHide() const {
  return window_->IsMenuBarAutoHide();
}

bool BaseWindow::IsMenuBarVisible() const {
  return window_->IsMenuBarVisible();
}

void BaseWindow::SetVibrancy(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             gin_helper::Arguments* args) {
  std::string type;
  if (!value->IsNullOrUndefined()) {
    type = gin::V8ToString(isolate, value);
  }
  gin_helper::Dictionary options;
  int animation_duration_ms = 0;

  if (args->GetNext(&options)) {
    options.Get("animationDuration", &animation_duration_ms);
  }

  window_->SetVibrancy(type, animation_duration_ms);
}

#if BUILDFLAG(IS_MAC)
std::string BaseWindow::GetAlwaysOnTopLevel() const {
  return window_->GetAlwaysOnTopLevel();
}

std::string BaseWindow::GetVibrancyTypeForTesting() const {
  return window_->GetVibrancyTypeForTesting();
}

bool BaseWindow::HasVibrancyView() const {
  return window_->HasVibrancyView();
}

std::string BaseWindow::GetVisualEffectStateForTesting() const {
  return window_->GetVisualEffectStateForTesting();
}

std::string BaseWindow::GetNativeVisualEffectStateForTesting() const {
  return window_->GetNativeVisualEffectStateForTesting();
}

void BaseWindow::SetWindowButtonVisibility(bool visible) {
  window_->SetWindowButtonVisibility(visible);
}

bool BaseWindow::GetWindowButtonVisibility() const {
  return window_->GetWindowButtonVisibility();
}

void BaseWindow::SetTrafficLightPosition(const gfx::Point& position) {
  // For backward compatibility we treat (0, 0) as resetting to default.
  if (position.IsOrigin()) {
    window_->SetTrafficLightPosition(std::nullopt);
  } else {
    window_->SetTrafficLightPosition(position);
  }
}

gfx::Point BaseWindow::GetTrafficLightPosition() const {
  // For backward compatibility we treat default value as (0, 0).
  return window_->GetTrafficLightPosition().value_or(gfx::Point());
}

bool BaseWindow::IsHiddenInMissionControl() const {
  return window_->IsHiddenInMissionControl();
}

void BaseWindow::SetHiddenInMissionControl(bool hidden) {
  window_->SetHiddenInMissionControl(hidden);
}

v8::Local<v8::Value> BaseWindow::GetTabbingIdentifier() const {
  auto tabbing_identifier = window_->GetTabbingIdentifier();
  if (!tabbing_identifier.has_value()) {
    return v8::Undefined(isolate());
  }
  return gin::StringToV8(isolate(), tabbing_identifier.value());
}

void BaseWindow::AddTabbedWindow(v8::Local<v8::Value> value,
                                 gin_helper::Arguments* args) {
  gin_helper::Handle<BaseWindow> window;
  if (!gin::ConvertFromV8(isolate(), value, &window) || window.IsEmpty()) {
    args->ThrowError("Must pass BaseWindow instance");
    return;
  }

  window_->AddTabbedWindow(window->window());
}

void BaseWindow::SelectPreviousTab() {
  window_->SelectPreviousTab();
}

void BaseWindow::SelectNextTab() {
  window_->SelectNextTab();
}

void BaseWindow::ShowAllTabs() {
  window_->ShowAllTabs();
}

void BaseWindow::MergeAllWindows() {
  window_->MergeAllWindows();
}

void BaseWindow::MoveTabToNewWindow() {
  window_->MoveTabToNewWindow();
}

void BaseWindow::ToggleTabBar() {
  window_->ToggleTabBar();
}
#endif

void BaseWindow::SetTouchBar(
    std::vector<gin_helper::PersistentDictionary> items) {
  window_->SetTouchBar(std::move(items));
}

void BaseWindow::RefreshTouchBarItem(const std::string& item_id) {
  window_->RefreshTouchBarItem(item_id);
}

void BaseWindow::SetEscapeTouchBarItem(gin_helper::PersistentDictionary item) {
  window_->SetEscapeTouchBarItem(std::move(item));
}

void BaseWindow::SetAspectRatio(double aspect_ratio,
                                gin_helper::Arguments* args) {
  gfx::Size extra_size;
  args->GetNext(&extra_size);
  window_->SetAspectRatio(aspect_ratio, extra_size);
}

v8::Local<v8::Value> BaseWindow::GetParentWindow() const {
  if (parent_window_.IsEmpty()) {
    return v8::Null(isolate());
  } else {
    return v8::Local<v8::Value>::New(isolate(), parent_window_);
  }
}

std::vector<v8::Local<v8::Object>> BaseWindow::GetChildWindows() const {
  return child_windows_.Values(isolate());
}

bool BaseWindow::IsModal() const {
  return window_->is_modal();
}

#if defined(OS_WIN)
bool BaseWindow::HookWindowMessage(UINT message,
                                   const MessageCallback& callback) {
  messages_callback_map_[message] = callback;
  return true;
}

void BaseWindow::UnhookWindowMessage(UINT message) {
  messages_callback_map_.erase(message);
}

bool BaseWindow::IsWindowMessageHooked(UINT message) {
  return messages_callback_map_.contains(message);
}

void BaseWindow::UnhookAllWindowMessages() {
  messages_callback_map_.clear();
}

#endif

int32_t BaseWindow::GetID() const {
  return weak_map_id();
}

void BaseWindow::RemoveFromParentChildWindows() {
  if (!parent_window_.IsEmpty()) {
    gin_helper::Handle<BaseWindow> parent;
    if (!gin::ConvertFromV8(isolate(), GetParentWindow(), &parent) ||
        parent.IsEmpty()) {
      return;
    }
    parent->child_windows_.Remove(weak_map_id());
  }
}

// static
gin_helper::WrappableBase* BaseWindow::New(gin_helper::Arguments* args) {
  gin_helper::Dictionary options =
      gin::Dictionary::CreateEmpty(args->isolate());
  args->GetNext(&options);

  return new BaseWindow(args, options);
}

// static
void BaseWindow::BuildPrototype(v8::Isolate* isolate,
                                v8::Local<v8::FunctionTemplate> prototype) {
  prototype->SetClassName(gin::StringToV8(isolate, "BaseWindow"));
  gin_helper::Destroyable::MakeDestroyable(isolate, prototype);
  gin_helper::ObjectTemplateBuilder(isolate, prototype->PrototypeTemplate())
      .SetMethod("close", &BaseWindow::Close)
      .SetMethod("focus", &BaseWindow::Focus)
      .SetMethod("blur", &BaseWindow::Blur)
      .SetMethod("isFocused", &BaseWindow::IsFocused)
      .SetMethod("show", &BaseWindow::Show)
      .SetMethod("showInactive", &BaseWindow::ShowInactive)
      .SetMethod("hide", &BaseWindow::Hide)
      .SetMethod("isVisible", &BaseWindow::IsVisible)
      .SetMethod("isEnabled", &BaseWindow::IsEnabled)
      .SetMethod("setEnabled", &BaseWindow::SetEnabled)
      .SetMethod("maximize", &BaseWindow::Maximize)
      .SetMethod("unmaximize", &BaseWindow::Unmaximize)
      .SetMethod("isMaximized", &BaseWindow::IsMaximized)
      .SetMethod("minimize", &BaseWindow::Minimize)
      .SetMethod("restore", &BaseWindow::Restore)
      .SetMethod("isMinimized", &BaseWindow::IsMinimized)
      .SetMethod("setFullScreen", &BaseWindow::SetFullScreen)
      .SetMethod("isFullScreen", &BaseWindow::IsFullscreen)
      .SetMethod("setBounds", &BaseWindow::SetBounds)
      .SetMethod("getBounds", &BaseWindow::GetBounds)
      .SetMethod("isNormal", &BaseWindow::IsNormal)
      .SetMethod("getNormalBounds", &BaseWindow::GetNormalBounds)
      .SetMethod("setSize", &BaseWindow::SetSize)
      .SetMethod("getSize", &BaseWindow::GetSize)
      .SetMethod("setContentBounds", &BaseWindow::SetContentBounds)
      .SetMethod("getContentBounds", &BaseWindow::GetContentBounds)
      .SetMethod("setContentSize", &BaseWindow::SetContentSize)
      .SetMethod("getContentSize", &BaseWindow::GetContentSize)
      .SetMethod("setMinimumSize", &BaseWindow::SetMinimumSize)
      .SetMethod("getMinimumSize", &BaseWindow::GetMinimumSize)
      .SetMethod("setMaximumSize", &BaseWindow::SetMaximumSize)
      .SetMethod("getMaximumSize", &BaseWindow::GetMaximumSize)
      .SetMethod("setSheetOffset", &BaseWindow::SetSheetOffset)
      .SetMethod("moveTop", &BaseWindow::MoveTop)
      .SetMethod("setResizable", &BaseWindow::SetResizable)
      .SetMethod("isResizable", &BaseWindow::IsResizable)
      .SetMethod("setMovable", &BaseWindow::SetMovable)
      .SetMethod("isMovable", &BaseWindow::IsMovable)
      .SetMethod("setMinimizable", &BaseWindow::SetMinimizable)
      .SetMethod("isMinimizable", &BaseWindow::IsMinimizable)
      .SetMethod("setMaximizable", &BaseWindow::SetMaximizable)
      .SetMethod("isMaximizable", &BaseWindow::IsMaximizable)
      .SetMethod("setFullScreenable", &BaseWindow::SetFullScreenable)
      .SetMethod("isFullScreenable", &BaseWindow::IsFullScreenable)
      .SetMethod("setClosable", &BaseWindow::SetClosable)
      .SetMethod("isClosable", &BaseWindow::IsClosable)
      .SetMethod("setAlwaysOnTop", &BaseWindow::SetAlwaysOnTop)
      .SetMethod("isAlwaysOnTop", &BaseWindow::IsAlwaysOnTop)
      .SetMethod("center", &BaseWindow::Center)
      .SetMethod("setPosition", &BaseWindow::SetPosition)
      .SetMethod("getPosition", &BaseWindow::GetPosition)
      .SetMethod("setTitle", &BaseWindow::SetTitle)
      .SetMethod("setIcon", &BaseWindow::SetIcon)
      .SetMethod("getTitle", &BaseWindow::GetTitle)
      .SetMethod("flashFrame", &BaseWindow::FlashFrame)
      .SetMethod("setProgressBar", &BaseWindow::SetProgressBar)
      .SetMethod("setSkipTaskbar", &BaseWindow::SetSkipTaskbar)
      .SetMethod("setAutoHideMenuBar", &BaseWindow::SetAutoHideMenuBar)
      .SetMethod("isMenuBarAutoHide", &BaseWindow::IsMenuBarAutoHide)
      .SetMethod("isMenuBarVisible", &BaseWindow::IsMenuBarVisible)
      .SetMethod("setSimpleFullScreen", &BaseWindow::SetSimpleFullScreen)
      .SetMethod("isSimpleFullScreen", &BaseWindow::IsSimpleFullScreen)
      .SetMethod("setBackgroundColor", &BaseWindow::SetBackgroundColor)
      .SetMethod("getBackgroundColor", &BaseWindow::GetBackgroundColor)
      .SetMethod("setHasShadow", &BaseWindow::SetHasShadow)
      .SetMethod("hasShadow", &BaseWindow::HasShadow)
      .SetMethod("setOpacity", &BaseWindow::SetOpacity)
      .SetMethod("getOpacity", &BaseWindow::GetOpacity)
      .SetMethod("setFocusable", &BaseWindow::SetFocusable)
      .SetMethod("isFocusable", &BaseWindow::IsFocusable)
      .SetMethod("setParentWindow", &BaseWindow::SetParentWindow)
      .SetMethod("getNativeWindowHandle", &BaseWindow::GetNativeWindowHandle)
      .SetMethod("setVisibleOnAllWorkspaces",
                 &BaseWindow::SetVisibleOnAllWorkspaces)
      .SetMethod("isVisibleOnAllWorkspaces",
                 &BaseWindow::IsVisibleOnAllWorkspaces)
#if BUILDFLAG(IS_MAC)
      .SetMethod("_getAlwaysOnTopLevel", &BaseWindow::GetAlwaysOnTopLevel)
      .SetMethod("_getVibrancyType", &BaseWindow::GetVibrancyTypeForTesting)
      .SetMethod("_hasVibrancyView", &BaseWindow::HasVibrancyView)
      .SetMethod("_getVisualEffectState",
                 &BaseWindow::GetVisualEffectStateForTesting)
      .SetMethod("_getNativeVisualEffectState",
                 &BaseWindow::GetNativeVisualEffectStateForTesting)
      .SetMethod("setAutoHideCursor", &BaseWindow::SetAutoHideCursor)
      .SetMethod("isHiddenInMissionControl",
                 &BaseWindow::IsHiddenInMissionControl)
      .SetMethod("setHiddenInMissionControl",
                 &BaseWindow::SetHiddenInMissionControl)
      .SetMethod("addTabbedWindow", &BaseWindow::AddTabbedWindow)
      .SetMethod("selectPreviousTab", &BaseWindow::SelectPreviousTab)
      .SetMethod("selectNextTab", &BaseWindow::SelectNextTab)
      .SetMethod("showAllTabs", &BaseWindow::ShowAllTabs)
      .SetMethod("mergeAllWindows", &BaseWindow::MergeAllWindows)
      .SetMethod("moveTabToNewWindow", &BaseWindow::MoveTabToNewWindow)
      .SetMethod("toggleTabBar", &BaseWindow::ToggleTabBar)
#endif
      .SetMethod("setVibrancy", &BaseWindow::SetVibrancy)
#if BUILDFLAG(IS_MAC)
      .SetMethod("setTrafficLightPosition",
                 &BaseWindow::SetTrafficLightPosition)
      .SetMethod("getTrafficLightPosition",
                 &BaseWindow::GetTrafficLightPosition)
#endif
      .SetMethod("_setTouchBarItems", &BaseWindow::SetTouchBar)
      .SetMethod("_refreshTouchBarItem", &BaseWindow::RefreshTouchBarItem)
      .SetMethod("_setEscapeTouchBarItem", &BaseWindow::SetEscapeTouchBarItem)
#if BUILDFLAG(IS_MAC)
      .SetMethod("setWindowButtonVisibility",
                 &BaseWindow::SetWindowButtonVisibility)
      .SetMethod("_getWindowButtonVisibility",
                 &BaseWindow::GetWindowButtonVisibility)
      .SetProperty("tabbingIdentifier", &BaseWindow::GetTabbingIdentifier)
      .SetProperty("excludedFromShownWindowsMenu",
                   &BaseWindow::IsExcludedFromShownWindowsMenu,
                   &BaseWindow::SetExcludedFromShownWindowsMenu)
#endif
      .SetMethod("setAspectRatio", &BaseWindow::SetAspectRatio)
      .SetMethod("getParentWindow", &BaseWindow::GetParentWindow)
      .SetMethod("getChildWindows", &BaseWindow::GetChildWindows)
      .SetMethod("isModal", &BaseWindow::IsModal)
#if BUILDFLAG(IS_WIN)
      .SetMethod("hookWindowMessage", &BaseWindow::HookWindowMessage)
      .SetMethod("isWindowMessageHooked", &BaseWindow::IsWindowMessageHooked)
      .SetMethod("unhookWindowMessage", &BaseWindow::UnhookWindowMessage)
      .SetMethod("unhookAllWindowMessages",
                 &BaseWindow::UnhookAllWindowMessages)
#endif
      .SetProperty("id", &BaseWindow::GetID);
}

void BaseWindow::SetProgressBar(double progress, gin_helper::Arguments* args) {
  gin_helper::Dictionary options;
  std::string mode = "normal";
  if (args->GetNext(&options)) {
    options.Get("mode", &mode);
  }

  NativeWindow::ProgressState state = NativeWindow::ProgressState::kNormal;
  if (mode == "none") {
    state = NativeWindow::ProgressState::kNone;
  } else if (mode == "indeterminate") {
    state = NativeWindow::ProgressState::kIndeterminate;
  } else if (mode == "error") {
    state = NativeWindow::ProgressState::kError;
  } else if (mode == "paused") {
    state = NativeWindow::ProgressState::kPaused;
  } else if (mode == "normal") {
    state = NativeWindow::ProgressState::kNormal;
  }

  window_->SetProgressBar(progress, state);
}

}  // namespace lynxtron::api

namespace {

using lynxtron::api::BaseWindow;

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  BaseWindow::SetConstructor(isolate, base::BindRepeating(&BaseWindow::New));

  gin_helper::Dictionary constructor(isolate,
                                     BaseWindow::GetConstructor(isolate)
                                         ->GetFunction(context)
                                         .ToLocalChecked());
  constructor.SetMethod("fromId", &BaseWindow::FromWeakMapID);
  constructor.SetMethod("getAllWindows", &BaseWindow::GetAll);

  gin_helper::Dictionary dict(isolate, exports);
  dict.Set("BaseWindow", constructor);
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(lynxtron_base_window, Initialize)
